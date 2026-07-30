#ifndef STRING_H
#define STRING_H

#include <stdint.h>

void strcpy(char *dst, const char *src);

void memcpy(void *dst,const void *src,uint64_t size);

uint64_t strlen(const char *str);

int strcmp(const char *a,const char *b);

uint32_t StringHexToUInt32(const char *String, uint32_t *Value);

void *memset(void *dest, int value, uint64_t size);

#endif