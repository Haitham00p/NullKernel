#ifndef NULLOS_VFS_H
#define NULLOS_VFS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    const char *Name;
    const void *Data;
    uint64_t Size;
} VfsFileInfo;

typedef bool (*VfsListCallback)(const VfsFileInfo *File, void *Context);

/* The initial VFS mounts the in-memory filesystem at /. */
void VfsInitialize(void);
bool VfsCreate(const char *Path);
bool VfsWrite(const char *Path, const void *Data, uint64_t Size);
bool VfsRead(const char *Path, const void **Data, uint64_t *Size);
bool VfsRemove(const char *Path);
bool VfsList(VfsListCallback Callback, void *Context);

#endif
