#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>

//indexes a file for determining changes
struct filenode {
	char			*filename; //file's path name
	long 			last_modify_time, size; //metadata
	struct filenode *next; //used to resolve hashing collisions
};

//hash table of files filename -> filenode
struct cache {
	size_t 			capacity; //size of underlying hash table
	struct filenode **values;  //values stored in hash table		
};

/*
 * Create a filenode for a given file
 */
struct filenode *init_filenode(char *filename);

/*
 * Free a filenode from the heap
 */
void free_node(struct filenode *node);

/*
 * Initialize a cache with a given capacity on the heap
 */
struct cache *init_cache(size_t initial_capacity);

/*
 * Reclaim the memory stored by a cache
 */
void free_cache(struct cache *cache);

/*
 * Hash a filename for inserting into the cache
 */
size_t hash(struct cache *cache, char *filename);

/*
 * Insert a file into the cache
 * Returns 0 if the file was inserted
 * Otherwise returns -1
 */
int insert(struct cache *cache, char *filename);

/*
 * Get a filenode from the cache
 * Returns filenode the file is found
 * Otherwise returns 0 
 */
struct filenode *get(struct cache *cache, char *filename);

/*
 * Delete a file from the cache
 * Returns 0 if the file was found and deleted
 * Otherwise returns -1
 */
int delete(struct cache *cache, char *filename);

#endif
