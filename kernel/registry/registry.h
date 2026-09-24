#ifndef NULLOS_REGISTRY_H
#define NULLOS_REGISTRY_H
#include <stdint.h>
#include <stdbool.h>
bool RegistryInitialize(void);
bool RegistryEnsureLayout(void);
bool RegistryReadText(const char *KeyPath, char *Out, uint32_t MaxOut);
bool RegistryWriteText(const char *KeyPath, const char *Value);
bool RegistryReadU64(const char *KeyPath, uint64_t *Value);
bool RegistryWriteU64(const char *KeyPath, uint64_t Value);
bool RegistryPathExists(const char *KeyPath);
uint32_t RegistryVersion(void);
#endif
