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

#endif
