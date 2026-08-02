#ifndef STRING_H
#define STRING_H
#include <stdint.h>
#include <stdlib.h>
int  strcmp     ( char * s1, char * s2 );
void strset     ( char * s1, int c, int size );
int  strlen     ( char * s );
void itoa       ( int x, char str[], int d);
void ftoa       ( float n, char* res, int afterpoint ); 
unsigned long utils_atoi(const char *s, int char_size);
size_t utils_strlen(const char *s);
void reverse    ( char *s );
void itohex_str( uint64_t d, int size, char * s );
void utils_strcpy(char *dst, char * src);
char * utils_strdup(const char *s);

#endif