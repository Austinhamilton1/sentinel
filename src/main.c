#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "cache.h"
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
void clean_cache(struct cache *cache);

/*
 * Used to ensure the program exits gracefully (frees all memory)
 * when killed
 */
void handle_signal(int sig);

/*
 * Debug function to print a filenode
 */
void print_filenode(struct filenode *node);

struct cache *cache = 0;

int main(int argc, char *argv[]) {
	if(argc != 3) {
		printf("Usage: sentinel [src_path] [dest_path]\n");
		return -1;
	}

	//result of building a new cache
	int init_result;

	//initialize a cache with a capacity of 400
	cache = init_cache(400);

	//try to build the cache
	init_result = build_cache(cache, argv[1]);
	if(init_result < 0) 
		printf("Failed to build cache.\n");
	else
		printf("Cache build successfully.\n");

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	while(1) {}

	return 0;
}

int build_cache(struct cache *cache, char *path) {
	int insert_result, fd, st_res;
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
		printf("Failed to insert %s\n", path);
		return -1;
	}
	
	//open the file, checking for errors
	if((fd = open(path, O_RDONLY)) < 0) {
		printf("Failed to open %s\n", path);
		return -1;
	}
	
	//stat the file, checking for errors
	if((st_res = fstat(fd, &st_info)) < 0) {
		printf("Failed to stat %s\n", path);
		close(fd);
		return -1;
	}

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
				int result = build_cache(cache, buf);
				if(result < 0)
					return -1;
			}
		}
	}

	//cleanup
	close(fd);
	return 0; //success
}

void clean_cache(struct cache *cache) {
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
	int insert_result, fd, st_res, updated;
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
		if((fd = open(path, O_RDONLY)) < 0) {
			printf("Couldn't open file: %s\n", path);
			return -1;
		}

		//stat the file, checking for errors
		if((st_res = fstat(fd, &st_info)) < 0) {
			printf("Couldn't stat file: %s\n", path);
			close(fd);
			return -1;
		}

		//check to see if the entry needs to be updated
		updated = 0;
		if(st_info.st_mtime != filenode->last_modify_time || st_info.st_size != filenode->size) {
			filenode->last_modify_time = st_info.st_mtime;
			filenode->size = st_info.st_size;
			updated = 1;
		}

		close(fd);
	}
	else {
		//insert the file into the cache
		insert_result = insert(cache, path);
		if(insert_result < 0) {
			printf("Couldn't insert file: %s\n", path);
			return -1;
		}
		updated = 1;
	}
	
	//if the file was updated, we need to also recursively update
	//its children if it is a directory
	if(updated == 1) {
		//open the file, checking for errors
		if((fd = open(path, O_RDONLY)) < 0) {
			printf("Couldn't open file: %s\n", path);
			return -1;
		}

		//stat the file, checking for errors
		if((st_res = fstat(fd, &st_info)) < 0) {
			printf("Couldn't stat file: %s\n", path);
			close(fd);
			return -1;
		}

		//if the file is a directory, update its children
		if(S_ISDIR(st_info.st_mode)) {
			DIR *dp;
			struct dirent *ep;

			dp = opendir(path);
			while((ep = readdir(dp)) != 0) {
				//get the full path
				char buf[4096];
				memset(buf, 0, 4096);
				join(buf, path, ep->d_name, 4095);

				//try to update the cache recursively,
				//if it fails, fail all the way up
				int result = update_cache(cache, buf);
				if(result < 0) {
					close(fd);
					return -1;
				}
			}
		}
	}

	//cleanup
	close(fd);
	return 0;
}

void handle_signal(int sig) {
	free_cache(cache);	
	exit(0);
}
