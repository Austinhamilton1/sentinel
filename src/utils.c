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
