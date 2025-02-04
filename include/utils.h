#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/*
 * Join two strings together in a destination buffer
 */
void join(char *dest, char *src1, char *src2, size_t maxlen);

/*
 * Raise a base to a power while ensuring mod constraint
 */
size_t mod_exp(size_t base, size_t exp, size_t mod);

/*
 * Get just the filename of a full path
 */
void filename(char *result, char *path, size_t maxlen);

/*
 * Get the relative file name of a full path compared to parent
 */
void relative(char *result, char *path, char *parent, size_t maxlen);

/*
 * Copy the file stored at the r_fd file descriptor to the file
 * stored at the w_fd descriptor
 */
int copy(int w_fd, int r_fd, int bufsize);

/*
 * Remove a file or directory recursively
 */
int rm(char *path);

#endif
