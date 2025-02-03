#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <stdio.h>

#include "cache.h"
#include "utils.h"

struct filenode *init_filenode(char *filename) {
	int fd, st_res; //results for system calls
	struct stat st_info; //stat info
	
	if((fd = open(filename, O_RDONLY)) < 0)
		return 0;
	
	if((st_res = fstat(fd, &st_info)) < 0) {
		close(fd);
		return 0;
	}

	//create the filenode
	struct filenode *node = (struct filenode *)malloc(sizeof(struct filenode));
	if(node == 0)
		return 0;

	//copy filename to the filenode struct
	node->filename = strndup(filename, 4096);
	if(node->filename == 0) {
		free(node);
		return 0;
	}

	//copy data to filenode struct
	node->last_modify_time = st_info.st_mtime;
	node->size = st_info.st_size;
	if(S_ISREG(st_info.st_mode))
		node->type = FILE_TYPE_FILE;
	else if(S_ISDIR(st_info.st_mode))
		node->type = FILE_TYPE_DIR;	
	node->next = 0;

	//cleanup
	close(fd);

	return node;
}

void free_node(struct filenode *node) {
	//check if the node is not null
	if(node != 0) {
		//check if the filename is not null
		if(node->filename != 0)
			free(node->filename);
		free(node);
	}
}

struct cache *init_cache(size_t initial_capacity) {
	struct cache *cache = (struct cache *)malloc(sizeof(struct cache));

	//check if the malloc didn't work
	if(cache == 0)
		return 0;

	//initialize cache
	cache->capacity = initial_capacity;
	cache->values = (struct filenode **)malloc(initial_capacity * sizeof(struct filenode *));
	
	//check if hash table malloc didn't work
	if(cache->values == 0)
		free(cache);
	
	//used so we can check if the table is empty
	for(size_t i = 0; i < initial_capacity; i++)
		cache->values[i] = 0;

	return cache;
}

void free_cache(struct cache *cache) {
	//free the cache and its hash table if they are not null
	if(cache != 0) {
		if(cache->values != 0) {
			//free the hash table
			for(size_t i = 0; i < cache->capacity; i++) {
				struct filenode *ptr, *tmp;
				tmp = 0;
				ptr = cache->values[i];

				//free the linked list at values[i]
				while(ptr != 0) {
					tmp = ptr->next;
					free_node(ptr);
					ptr = tmp;
				}
			}
			free(cache->values);
		}
		free(cache);
	}
}

size_t hash(struct cache *cache, char *filename) {
	//this is a rolling polynomial hash function
	long p = 67;
	long i = 0;
	size_t hash = 0;
	for(char *ptr = filename; *ptr != 0; ptr++, i++)
		hash = (hash + (*ptr * mod_exp(p, i, cache->capacity))) % cache->capacity; 
	return hash % cache->capacity;
}

int insert(struct cache *cache, char *filename) {
	//create key and value
	size_t h = hash(cache, filename);
	struct filenode *filenode = init_filenode(filename);

	//indicate failure if we couldn't create the node
	if(filenode == 0)
		return -1;

	//there is already an entry for the key filename
	struct filenode *existing = get(cache, filename);
	if(existing != 0) {
		existing->last_modify_time = filenode->last_modify_time;
		existing->size = filenode->size;
		free_node(filenode);
		return 0;
	}

	//there is not an entry for the key filename
	struct filenode *ptr, *prev;
	ptr = cache->values[h];
	prev = 0;

	//append filenode to the end of the list
	while(ptr != 0) {
		prev = ptr;
		ptr = ptr->next;
	}

	//no linked list for values[h]
	if(prev == 0)
		cache->values[h] = filenode;
	//linked list exists for values[h]
	else
		prev->next = filenode;
	
	return 0;
}

struct filenode *get(struct cache *cache, char *filename) {
	//index into the hash table
	size_t h = hash(cache, filename);

	//hash table has no entries
	if(cache->values[h] == 0)
		return 0;

	//traverse the hash table linked list at h until a match is found
	struct filenode *ptr = cache->values[h];
	while(ptr != 0 && strcmp(ptr->filename, filename) != 0)
		ptr = ptr->next;
	return ptr;
}

int delete(struct cache *cache, char *filename) {
	//index into the hash table
	size_t h = hash(cache, filename);

	if(cache->values[h] == 0)
		return -1;

	//traverse the hash table linked list at h until a match is found
	struct filenode *ptr, *prev;
	ptr = cache->values[h];
	prev = 0;
	while(ptr != 0 && strcmp(ptr->filename, filename) != 0) {
		prev = ptr;
		ptr = ptr->next;
	}

	//key filename not found
	if(ptr == 0)
		return -1;

	//key filename is root of linked list at index h
	if(prev == 0)
		cache->values[h] = ptr->next;
	//key filename is not root of linked list at index h
	else
		prev->next = ptr->next;

	free_node(ptr);
	return 0;
}
