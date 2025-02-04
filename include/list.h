#ifndef LIST_H
#define LIST_H

#include <stddef.h>

struct list {
    char **values;
    size_t length, capacity;
};

/*
 * Initialize a list with a starting capacity
 */
struct list *init_list(size_t capacity);

/*
 * Free a list back to the heap
 */
void free_list(struct list *list);

/*
 * Get the value at index i
 */
int fetch(struct list *list, char *dest, size_t i, size_t maxlen);

/*
 * Add an item to the list
 */
int append(struct list *list, char *src);

/*
 * Clear a list
 */
void clear(struct list *list);

/*
 * Sort a list by the length of its contents
 */
void sortlen(struct list *list);

#endif