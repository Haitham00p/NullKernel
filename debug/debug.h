#ifndef DEBUG_H
#define DEBUG_H
#include <stdint.h>

void DbgBoot(const char *str, uintptr_t *API);
void DbgInfo(const char *str, uintptr_t *API);
void DbgErr(const char *str, uintptr_t *API);
void DbgWarn(const char *str, uintptr_t *API);

#endif