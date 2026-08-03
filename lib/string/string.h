#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stdbool.h>

void strcpy(char *dst, const char *src);
void strncpy(char *dst, const char *src, uint64_t n);

void memcpy(void *dst, const void *src, uint64_t size);

uint64_t strlen(const char *str);

int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, uint64_t n);

char tolower_c(char c);
char toupper_c(char c);

uint32_t StringHexToUInt32(const char *String, uint32_t *Value);

void *memset(void *dest, int value, uint64_t size);

#endif