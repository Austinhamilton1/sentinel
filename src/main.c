#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "cache.h"
#include "list.h"
#include "utils.h"

/*
 * Build a cache from a path
 * If the path is a directory add all subfiles
 * and subdirectories to the cache recursively
 */
int build_cache(struct cache *cache, char *path);

/*
 * Update the cache from a path
 * Remove all deleted files, add any new files, update metadata of existing
 * files
 */
int update_cache(struct cache *cache, char *path);

/*
 * Remove all files from the cache that no longer exist
 */
void clean_cache(struct cache *cache, struct list *list);

/*
 * Migrate a src to a destination on the same physical filesystem
 */
int migrate_phy(char *src, char *dest);

/*
 * Synchronize two folders on the same physical filesystem
 */
int sync_phy(struct cache *cache, struct list *list, char *src, char *dest);

/*
 * Frees all used memory and file descriptors
 */
void cleanup();

/*
 * Used to ensure the program exits gracefully (frees all memory)
 * when killed
 */
void handle_signal(int sig);

//Just for debugging. Remove this after production
void print_filenode(struct filenode *filenode);

struct cache *cache = 0;
struct list *list = 0;
int r_fd, w_fd = -1;

int main(int argc, char *argv[]) {
	if(argc != 3) {
		printf("Usage: sentinel [src_path] [dest_path]\n");
		return -1;
	}

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	//initialize a cache with a capacity of 400
	cache = init_cache(400);

	printf("Building cache...");
	//try to build the cache
	if(build_cache(cache, argv[1]) < 0) {
		printf("Failed.\n");
		cleanup();
		return -1;
	}
	printf("OK.\n");

	printf("Migrating files...");
	if(migrate_phy(argv[1], argv[2]) < 0) {
		printf("Failed.\n");
		cleanup();
		return -1;
	}

	printf("OK.\n");
	
	while(1) {
		list = init_list(400);
		if(list == 0) {
			fprintf(stderr, "Couldn't create list.\n");
			cleanup();
			return -1;
		}
		clean_cache(cache, list);
		if(update_cache(cache, argv[1]) < 0)
			fprintf(stderr, "Update failed.\n");
		sleep(1);
		if(sync_phy(cache, list, argv[1], argv[2]) < 0)
			fprintf(stderr, "Sync failed.\n");
		free_list(list);
	}

	return 0;
}

int build_cache(struct cache *cache, char *path) {
	int insert_result, st_res;
	struct stat st_info;

	char relative_filename[256];
	memset(relative_filename, 0, 256);
	filename(relative_filename, path, 255);

	if(strcmp(relative_filename, ".") == 0 || strcmp(relative_filename, "..") == 0)
		return 0;
	
	//try to insert the path into the cache
	insert_result = insert(cache, path);

	//if failed, show failure for entire operation
	if(insert_result == -1) {
		fprintf(stderr, "Error in building cache - Failed to insert %s\n", path);
		return -1;
	}
	
	//open the file, checking for errors
	if((r_fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "Error in building cache - Failed to open %s\n", path);
		return -1;
	}
	
	//stat the file, checking for errors
	if((st_res = fstat(r_fd, &st_info)) < 0) {
		fprintf(stderr, "Error in building cache - Failed to stat %s\n", path);
		close(r_fd);
		return -1;
	}

	//cleanup before recursion
	close(r_fd);

	//if the file is a directory, recursively build the cache
	//by expanding the directory
	if(S_ISDIR(st_info.st_mode)) {
		DIR *dp;
		struct dirent *ep;
		
		dp = opendir(path);
		if(dp != 0) {
			while((ep = readdir(dp)) != 0) {
				//we need absolute path here
				char buf[4096];
				memset(buf, 0, 4096);
				join(buf, path, ep->d_name, 4095);

				//if any of the build_dir calls fail, fail all
				//the way up
				if(build_cache(cache, buf) < 0) {
					closedir(dp);
					return -1;
				}
			}
			closedir(dp);
		}
	}

	//success
	return 0; 
}

void clean_cache(struct cache *cache, struct list *list) {
	if(cache == 0 || cache->values == 0)
		return;

	//loop through the hash table
	for(size_t i = 0; i < cache->capacity; i++) {
		struct filenode *ptr, *prev;
		ptr = cache->values[i];
		prev = 0;

		//loop through the linked lists
		while(ptr != 0) {
			//delete the filenode entry if the file doesn't exist
			if(access(ptr->filename, F_OK) != 0) {
				printf("File deleted: %s\n", ptr->filename);

				append(list, ptr->filename);

				struct filenode *tmp = ptr->next;

				//first entry
				if(prev == 0)
					cache->values[i] = tmp;
				//somewhere in the list
				else 
					prev->next = tmp;

				//free the filenode and then move to the next
				free_node(ptr);
				ptr = tmp;
			}
			else {
				//traverse the list normally
				prev = ptr;
				ptr = ptr->next;
			}
		}
	}
}

int update_cache(struct cache *cache, char *path) {
	int st_res;
	struct stat st_info;

	char relative_filename[256];
	memset(relative_filename, 0, 256);
	filename(relative_filename, path, 255);

	//don't check the current or parent directories
	if(strcmp(relative_filename, ".") == 0 || strcmp(relative_filename, "..") == 0)
		return 0;
	
	//try to insert the path into the cache
	struct filenode *filenode = get(cache, path);

	//file already in cache
	if(filenode != 0) {	
		//open the file, checking for errors
		if((r_fd = open(path, O_RDONLY)) < 0) {
			fprintf(stderr, "Error in updating cache - Couldn't open file: %s\n", path);
			return -1;
		}

		//stat the file, checking for errors
		if((st_res = fstat(r_fd, &st_info)) < 0) {
			fprintf(stderr, "Error in updating cache - Couldn't stat file: %s\n", path);
			close(r_fd);
			return -1;
		}

		//check to see if the entry needs to be updated
		if(st_info.st_mtime != filenode->last_modify_time || st_info.st_size != filenode->size) {
			printf("File updated: %s\n", path);
			filenode->last_modify_time = st_info.st_mtime;
			filenode->size = st_info.st_size;
		}

		close(r_fd);
	}
	else {
		//insert the file into the cache
		if(insert(cache, path) < 0) {
			fprintf(stderr, "Error in updating cache - Couldn't insert file: %s\n", path);
			return -1;
		}
		printf("File created: %s\n", path);
	}
	
	//we need to also recursively update
	//its children if it is a directory

	//open the file, checking for errors
	if((r_fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "Error in updating cache - Couldn't open file: %s\n", path);
		return -1;
	}

	//stat the file, checking for errors
	if((st_res = fstat(r_fd, &st_info)) < 0) {
		fprintf(stderr, "Error in updating cache - Couldn't stat file: %s\n", path);
		close(r_fd);
		return -1;
	}

	//close before recursion
	close(r_fd);

	//if the file is a directory, update its children
	if(S_ISDIR(st_info.st_mode)) {
		DIR *dp;
		struct dirent *ep;

		dp = opendir(path);
		if(dp != 0) {
			while((ep = readdir(dp)) != 0) {
				//get the full path
				char buf[4096];
				memset(buf, 0, 4096);
				join(buf, path, ep->d_name, 4095);

				//try to update the cache recursively,
				//if it fails, fail all the way up
				if(update_cache(cache, buf) < 0) {
					closedir(dp);
					return -1;
				}
			}
			closedir(dp);
		}
	}

	//success
	return 0;
}

int migrate_phy(char *src, char *dest) {
	int st_res;
	struct stat st_info;

	char relative_filename[256];
	memset(relative_filename, 0, 256);
	filename(relative_filename, src, 255);

	//don't check the current directory or the parent directory
	if(strcmp(relative_filename, ".") == 0 || strcmp(relative_filename, "..") == 0)
		return 0;

	if((r_fd = open(src, O_RDONLY)) < 0) {
		fprintf(stderr, "Error in physical migration- Couldn't open file: %s\n", src);
		return -1;
	}

	if((st_res = fstat(r_fd, &st_info)) < 0) {
		fprintf(stderr, "Error in physical migration - Could not stat file: %s\n", src);
		close(r_fd);
		return -1;
	}

	//close before recursion
	close(r_fd);

	//directory
	if(S_ISDIR(st_info.st_mode)) {
		if(access(dest, F_OK) < 0) {
			mkdir(dest, st_info.st_mode);
		}

		DIR *dp;
		struct dirent *ep;

		dp = opendir(src);
		if(dp != 0) {
			while((ep = readdir(dp)) != 0) {
				//get the full path
				char srcbuf[4096], destbuf[4096];
				memset(srcbuf, 0, 4096);
				memset(destbuf, 0, 4096);
				join(srcbuf, src, ep->d_name, 4095);
				join(destbuf, dest, ep->d_name, 4095);

				//try to migrate the directory recursively,
				//if it fails, fail all the way up
				if(migrate_phy(srcbuf, destbuf) < 0) {
					closedir(dp);
					return -1;
				}
			}
			closedir(dp);
		}
	}
	//regular file (make sure it doesn't already exist so it doesn't write over existing data in the project)
	else if(S_ISREG(st_info.st_mode) && access(dest, F_OK) < 0) {
		if((r_fd = open(src, O_RDONLY)) < 0) {
			fprintf(stderr, "Error in physical migration - Could not open file: %s\n", src);
			return -1;
		}
		if((w_fd = open(dest, O_WRONLY | O_CREAT, st_info.st_mode)) < 0) {
			fprintf(stderr, "Error in physical migration - Could not open file: %s\n", dest);
			close(r_fd);
			return -1;
		}

		//copy source to destination
		if(copy(w_fd, r_fd, 256) < 0) {
			fprintf(stderr, "Error in physical migration - Could not copy files: %s -> %s\n", src, dest);
			close(r_fd);
			close(w_fd);
			return -1;
		}

		close(r_fd);
		close(w_fd);
	}

	return 0;
}

int sync_phy(struct cache *cache, struct list *list, char *src, char *dest) {
	int st_res;
	struct stat st_info;

	char relative_filename[256];
	memset(relative_filename, 0, 256);
	filename(relative_filename, src, 255);

	//don't check the current directory or the parent directory
	if(strcmp(relative_filename, ".") == 0 || strcmp(relative_filename, "..") == 0)
		return 0;

	struct filenode *filenode = get(cache, src);

	//delete any deleted files
	for(size_t i = 0; i < list->length; i++) {
		//get the file name
		char buf[4096];
		memset(buf, 0, 4096);
		fetch(list, buf, i, 4095);

		printf("Deleted file at index %ld: %s\n", i, buf);
		printf("src:  %s\n", src);

		//if the src is in the list of deleted files, delete the dest
		if(strcmp(buf, src) == 0) {
			if(rm(dest) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't delete file: %s\n", dest);
				return -1;
			}
			printf("Deleted file: %s\n", dest);
		}
	}

	//file is new
	if(filenode == 0) { //This works!
		if((r_fd = open(src, O_RDONLY)) < 0) {
			fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", src);
			return -1;
		}
		
		if((st_res = fstat(r_fd, &st_info)) < 0) {
			fprintf(stderr, "Error in physical sync - Couldn't stat file: %s\n", src);
			close(r_fd);
			return -1;
		}

		//close before recursion
		close(r_fd);

		if(S_ISDIR(st_info.st_mode)) { //this works!
			if(mkdir(dest, st_info.st_mode) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't make directory: %s\n", dest);
				return -1;
			}
			printf("Created directory: %s\n", dest);
		}
		else if(S_ISREG(st_info.st_mode)) { //this works!
			if((r_fd = open(src, O_RDONLY)) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", src);
				return -1;
			}
			if((w_fd = open(dest, O_WRONLY | O_CREAT, st_info.st_mode)) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", dest);
				close(r_fd);
				return -1;
			}

			//copy file 256 bytes at a time, checking for errors
			if(copy(w_fd, r_fd, 256) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't copy files: %s -> %s\n", src, dest);
				close(r_fd);
				close(w_fd);
				return -1;
			}
			close(r_fd);
			close(w_fd);

			printf("Created file: %s\n", dest);
		}
	}
	//file already exists (check for potential change)
	else {
		if((r_fd = open(src, O_RDONLY)) < 0) {
			fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", src);
			return -1;
		}

		if((st_res = fstat(r_fd, &st_info)) < 0) {
			fprintf(stderr, "Error in physical sync - Couldn't stat file: %s\n", src);
			close(r_fd);
			return -1;
		}

		//close before recursion
		close(r_fd);

		if((st_info.st_size != filenode->size || st_info.st_mtime != filenode->last_modify_time) && S_ISREG(st_info.st_mode)) {
			if((r_fd = open(src, O_RDONLY)) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", src);
				return -1;
			}
			if((w_fd = open(dest, O_WRONLY | O_TRUNC)) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't open file: %s\n", dest);
				close(r_fd);
				return -1;
			}

			//copy file 256 bytes at a time, checking for errors
			if(copy(w_fd, r_fd, 256) < 0) {
				fprintf(stderr, "Error in physical sync - Couldn't copy files: %s -> %s\n", src, dest);
				close(r_fd);
				close(w_fd);
				return -1;
			}
		}
	}

	//recurse to the next level
	if(S_ISDIR(st_info.st_mode)) {
		DIR *dp;
		struct dirent *ep;

		dp = opendir(src);
		if(dp != 0) {
			while((ep = readdir(dp)) != 0) {
				//get the full path
				char srcbuf[4096], destbuf[4096];
				memset(srcbuf, 0, 4096);
				memset(destbuf, 0, 4096);
				join(srcbuf, src, ep->d_name, 4095);
				join(destbuf, dest, ep->d_name, 4095);

				//try to sync the directory recursively,
				//if it fails, fail all the way up
				if(sync_phy(cache, list, srcbuf, destbuf) < 0) {
					closedir(dp);
					return -1;
				}
			}
			closedir(dp);
		}
	}

	//success
	return 0;
}

void cleanup() {
	printf("Stopping...");

	//free up allocated memory
	free_cache(cache);
	free_list(list);

	//close read file descriptor
	if(r_fd != -1 && fcntl(r_fd, F_GETFL) >= 0)	
		close(r_fd);

	//close write file descriptor
	if(w_fd != -1 && fcntl(w_fd, F_GETFL) >= 0)	
		close(w_fd);

	printf("OK\n");
}

void handle_signal(int sig) {
	cleanup(); //clean up
	exit(0); //exit gracefully
}

void print_filenode(struct filenode *filenode) {
	printf("File Name: %s\n", filenode->filename);
	printf("Last Modified: %ld\n", filenode->last_modify_time);
	printf("Size: %ld\n", filenode->size);
	if(filenode->type == FILE_TYPE_DIR)
		printf("File type: DIR\n");
	else if(filenode->type == FILE_TYPE_FILE)
		printf("File Type: FILE\n");
}