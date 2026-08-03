#ifndef NULLOS_DEBUG_H
#define NULLOS_DEBUG_H

#include <stdint.h>

void DbgBoot(const char *Message, uintptr_t Address);
void DbgInfo(const char *Message, uintptr_t Address);
void DbgErr(const char *Message, uintptr_t Address);
void DbgWarn(const char *Message, uintptr_t Address);

#endif
