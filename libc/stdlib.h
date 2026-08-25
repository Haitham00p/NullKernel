#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __NCC__

#define RAND_MAX 0x7FFFFFFF

void *malloc(size_t size);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

int    atoi(const char *s);
long   atol(const char *s);
long   strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);

int  abs(int x);
long labs(long x);

char *itoa(int value, char *buf, int base);
char *ltoa(long value, char *buf, int base);
char *utoa(unsigned int value, char *buf, int base);
char *ultoa(unsigned long value, char *buf, int base);

void srand(unsigned int seed);
int  rand(void);

#endif /* __NCC__ */

uint32_t ShellArgCount(void);
bool     ShellArgGet(uint32_t Index, char *Out, uint32_t MaxOut);
void     ShellApplyRegistry(void);
void     ShellApplyDefDir(void);
void     ShellBootBuild(void);
void     ShellBootRunRAB(void);
void     SelfHostExeName(const char *SourceName, char *ExeName);

/* n64a / nld toolchain entry points (shell commands and guests). */
bool SelfHostAssembleFile(const char *SourceName, const char *OutName);
bool SelfHostLinkFiles(const char **ObjectNames, uint32_t Count,
                       const char *OutName);

uint32_t readv(const char *Path, void *Buffer);
uint32_t writev(const char *Path, const char *Buffer);
uint32_t createK(const char *Path);
uint32_t editv(const char *Path, const char *Buffer);
uint64_t exec(const char *Path);
uint64_t execl(const char *fmt, ...);

#endif

const char *NccVaultGetHeader(const char *name);
bool NccVaultHasSymbol(const char *name);
void HighlightPrintFile(const char *FileName, const char *Text);

bool HlIsCKeyword(const char *s, uint32_t n);
bool HlIsCType(const char *s, uint32_t n);
bool HlIsAsmMnemonic(const char *s, uint32_t n);
bool HlIsAsmDirective(const char *s, uint32_t n);
bool HlIsAsmRegister(const char *s, uint32_t n);
bool HlIsAsmApi(const char *s, uint32_t n);
