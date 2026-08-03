#include "vfs.h"

#include "fs/ramfs/ramfs.h"
#include "lib/string/string.h"

static const char *VfsPathName(const char *Path)
{
    if (Path == 0 || Path[0] == '\0') return 0;
    if (Path[0] == '/') Path++;
    if (Path[0] == '\0') return 0;

    for (uint32_t Index = 0; Path[Index] != '\0'; Index++)
    {
        if (Path[Index] == '/') return 0;
    }
    return Path;
}

void VfsInitialize(void)
{
    RamFsInitialize();
}

bool VfsCreate(const char *Path)
{
    const char *Name = VfsPathName(Path);
    if (Name == 0 || RamFsFind(Name) != 0) return false;
    return RamFsCreate(Name) != 0;
}

bool VfsWrite(const char *Path, const void *Data, uint64_t Size)
{
    const char *Name = VfsPathName(Path);
    RamFile *File;

    if (Name == 0 || (Data == 0 && Size != 0)) return false;
    File = RamFsFind(Name);
    if (File == 0)
    {
        File = RamFsCreate(Name);
    }
    if (File == 0) return false;
    RamFsWrite(File, Data, Size);
    return true;
}

bool VfsRead(const char *Path, const void **Data, uint64_t *Size)
{
    const char *Name = VfsPathName(Path);
    RamFile *File;

    if (Name == 0 || Data == 0 || Size == 0) return false;
    File = RamFsFind(Name);
    if (File == 0) return false;
    *Data = File->Data;
    *Size = File->Size;
    return true;
}

bool VfsRemove(const char *Path)
{
    const char *Name = VfsPathName(Path);
    RamFile *File = Name == 0 ? 0 : RamFsFind(Name);
    if (File == 0) return false;
    RamFsDelete(File);
    return true;
}

bool VfsList(VfsListCallback Callback, void *Context)
{
    if (Callback == 0) return false;
    for (uint32_t Index = 0; Index < RAMFS_MAX_FILES; Index++)
    {
        RamFile *File = RamFsGetByIndex(Index);
        VfsFileInfo Info;
        if (File == 0) continue;
        Info.Name = File->Name;
        Info.Data = File->Data;
        Info.Size = File->Size;
        if (!Callback(&Info, Context)) break;
    }
    return true;
}
