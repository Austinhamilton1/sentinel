#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "list.h"

struct list *init_list(size_t capacity) {
    //allocate list
    struct list *list = (struct list *)malloc(sizeof(struct list));

    if(list == 0)
        return 0;

    //set aside memory for strings
    list->values = (char **)malloc(capacity * sizeof(char *));
    if(list->values == 0) {
        free(list);
        return 0;
    }

    //set capacity and length
    list->capacity = capacity;
    list->length = 0;

    return list;
}

void free_list(struct list *list) {
    if(list != 0) {
        if(list->values != 0) {
            //free stored strings
            for(size_t i = 0; i < list->length; i++) {
                if(list->values[i] != 0)
                    free(list->values[i]);
            }
            //free values
            free(list->values);
        }
        //free list
        free(list);
    }
}

int fetch(struct list *list, char *dest, size_t i, size_t maxlen) {
    //check if item exists
    if(list == 0 || list->values == 0 || list->values[i] == 0)
        return -1;

    //copy item
    memcpy(dest, list->values[i], maxlen);
    return 0;
}

int append(struct list *list, char *src) {
    if(list == 0 || list->values == 0)
        return -1;

    struct list *listptr = list;
    //if we have reached capacity, double the size of the list
    if(list->length == list->capacity) {
        struct list *newlist = init_list(list->capacity * 2);

        if(newlist == 0)
            return -1;

        //copy the old list
        for(size_t i = 0; i < list->length; i++)
            append(newlist, list->values[i]);
        listptr = newlist;
        free_list(list);
    }

    //copy the string to the list values
    list->values[listptr->length++] = strndup(src, 4096);
    list = listptr;

    return 0;
}

void clear(struct list *list) {
    //check if the list is null
    if(list == 0 || list->values == 0) 
        return;

    //clear out the list
    for(size_t i = 0; i < list->length; i++) {
        free(list->values[i]);
        list->values[i] = 0;
    }

    //reset the list length
    list->length = 0;
}

/*
 * Merge two list for merge sort
 */
static void merge(struct list *list, size_t start, size_t mid, size_t end) {
    size_t start2 = mid + 1;

    //if the direct merge is already sorted
    if(strlen(list->values[mid]) >= strlen(list->values[start2]))
        return; 

    //two pointers to maintain start of both arrays to merge
    while(start <= mid && start2 <= end) {
        //element 1 is in the right place
        if(strlen(list->values[start]) >= strlen(list->values[start2]))
            start++;
        else {
            char *value = list->values[start2];
            int index = start2;

            //shift all elements between element 1
            //element 2 right by 1
            while(index != start) {
                list->values[index] = list->values[index-1];
                index--;
            }
            list->values[start] = value;

            //update all the pointers
            start++;
            mid++;
            start2++;
        }
    }
}

/*
 * Merge sort algorithm
 */
static void mergesort(struct list *list, size_t l, size_t r) {
    if(l < r) {
        size_t m = l + (r-l) / 2;
        //merge first and second halves
        mergesort(list, l, m);
        mergesort(list, m+1, r);

        merge(list, l, m, r);
    }
}

void sortlen(struct list *list) {
    //check if the list is null
    if(list == 0 || list->values == 0 || list->length < 2)
        return;
    mergesort(list, 0, list->length-1);
}