#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"

void join(char *dest, char *src1, char *src2, size_t maxlen) {
	char *first = dest;
	size_t i = 0;

	//copy src1
	for(; *src1 != 0 && i < maxlen; i++) 
		*dest++ = *src1++;
	
	//if there is no slash between the two src, add a slash
	if(*(dest-1) != '/') {
		*dest++ = '/';
		i++;
	}

	//copy src2
	for(; *src2 != 0 && i < maxlen; i++)
		*dest++ = *src2++;

	dest = first;
}

size_t mod_exp(size_t base, size_t exp, size_t mod) {
	//this implementation is Exponentiation by Squaring
	size_t result = 1;
	base = base % mod; //Ensure base is within mod range

	while(exp > 0) {
		//if exp is odd, multiply base with result
		if(exp % 2 == 1)
			result = (result * base) % mod;

		//square the base
		base = (base * base) % mod;
		exp /= 2;
	}

	return result;
}

void filename(char *result, char *path, size_t maxlen) {
	//index of the last slash in the path name
	size_t last_slash = -1;
	size_t i = 0;

	//get the last slash index
	for(char *ptr = path; *ptr != 0; ptr++, i++) {
		if(*ptr == '/')
			last_slash = i;
	}

	//copy everything proceeding the last slash into result
	char *dest = result;
	i = 0;
	for(char *src = path + last_slash + 1; *src != 0 && i < maxlen; src++, i++)
		*dest++ = *src;
}

void relative(char *result, char *path, char *parent, size_t maxlen) {
	//index of the last character that matches in path and parent
	size_t i, j;
	i = 0;
	for(char *ptr = parent; *ptr != 0; ptr++)
		i++;

	if(path[i] == '/')
		i++;
	
	char *dest = result;
	j = 0;
	for(char *src = path + i; *src != 0 && j < maxlen; src++, j++)
		*dest++ = *src;
}

int copy(int w_fd, int r_fd, int bufsize) {
	char buf[bufsize];
	memset(buf, 0, bufsize);

	ssize_t chunk;
	while((chunk = read(r_fd, buf, bufsize)) > 0) {
		if(write(w_fd, buf, chunk) != chunk)
			return -1;
	}

	return 0;
}

int rm(char *path) {
	int fd, st_res;
	struct stat st_info;

	if((fd = open(path, O_RDONLY)) < 0)
		return -1;
	
	if((st_res = fstat(fd, &st_info)) < 0) {
		close(fd);
		return -1;
	}

	//no longer needed
	close(fd);
	
	if(S_ISDIR(st_info.st_mode)) 
		return rmdir(path);

	return unlink(path);
}

int ascending(char *a, char *b) {
	char *ptr1, *ptr2;

	ptr1 = a;
	ptr2 = b;
	while(*ptr1 == *ptr2) {
		if(*ptr1 == 0)
			return 0;
		ptr1++;
		ptr2++;
	}

	if(*ptr1 == 0)
		return -1;
	if(*ptr2 == 0)
		return 1;
	if(*ptr1 < *ptr2)
		return -1;
	return 1;
}

int descending(char *a, char *b) {
	char *ptr1, *ptr2;

	ptr1 = a;
	ptr2 = b;
	while(*ptr1 == *ptr2) {
		if(*ptr1 == 0)
			return 0;
		ptr1++;
		ptr2++;
	}

	if(*ptr1 == 0)
		return 1;
	if(*ptr2 == 0)
		return -1;
	if(*ptr1 > *ptr2)
		return -1;
	return 1;
}