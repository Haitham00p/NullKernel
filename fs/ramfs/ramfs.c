#include "kernel/terminal/terminal.h"
#include "lib/string/string.h"
#include "ramfs.h"
#include <stdint.h>
#include "mm/heap/heap.h"
#include <stddef.h>


RamFs fs;

void RamFsInitialize(void){
    fs.FileCount = 0;

    for (uint32_t i = 0; i < RAMFS_MAX_FILES; i++){
       fs.Files[i].Used = false;
       fs.Files[i].Name[0] = '\0';
       fs.Files[i].Data = NULL;
       fs.Files[i].Size = 0;
    }
}

RamFile *RamFsCreate(const char *Name){
    if (Name == NULL || Name[0] == '\0' || strlen(Name) >= RAMFS_MAX_FILE_LENGTH ||
        RamFsFind(Name) != NULL) return NULL;
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; i++){
        if (fs.Files[i].Used){
            continue;
        }    
        fs.Files[i].Data = NULL;
        fs.Files[i].Used = true;
        strcpy(fs.Files[i].Name, Name);
        fs.Files[i].Size = 0;
        fs.FileCount++;
        return &fs.Files[i];
        
    }
    return NULL;    
    
}

RamFile *RamFsGetByIndex(uint32_t Index)
{
    if (Index >= RAMFS_MAX_FILES || !fs.Files[Index].Used) return NULL;
    return &fs.Files[Index];
}

RamFile *RamFsFind(const char *Name)
{
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; i++)
    {
        if (!fs.Files[i].Used)
            continue;

        if (strcmp(fs.Files[i].Name, Name) == 0)
        {
            return &fs.Files[i];
        }
    }

    return NULL;
}

void RamFsWrite(RamFile *File, const void *Data, uint64_t Size){

    if (File == NULL) {
        TerminalWrite32("Error: ", 0x00FF0000);
        TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);
        return;
    }

    if (File->Used){
        void *ptr = kmalloc(Size + 1U);
        if (Size != 0) memcpy(ptr, Data, Size);
        ((char *)ptr)[Size] = '\0';
        File->Data = ptr;
        File->Size = Size;
        return;
    }
    TerminalWrite32("Error: ", 0x00FF0000);
    TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);


}

void RamFsPrint(RamFile *File){
    if (File == NULL){
        TerminalWrite32("Error: ", 0x00FF0000);
        TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);
        return;
    }
    if (File->Used){
        TerminalPrintLine32(File->Data, 0xFFFFFFFF);
        return;
    }
    TerminalWrite32("Error: ", 0x00FF0000);
    TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);
}

void RamFsDelete(RamFile *File){
    if (File == NULL){
        TerminalWrite32("Error: ", 0x00FF0000);
        TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);
        return;
    }
    if (File->Used){
        File->Data = NULL;
        memset(File->Name, 0, sizeof(File->Name));
        File->Used = false;
        File->Size = 0;
        fs.FileCount--;
        return;
    }
    TerminalWrite32("Error: ", 0x00FF0000);
    TerminalPrintLine32("File Not Registered", 0xFFFFFFFF);
}

void RamFsListFiles(){
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; i++){
        if (!fs.Files[i].Used){
            continue;
        }
        TerminalWrite32(fs.Files[i].Name, 0xFFFFFFFF);
        TerminalWrite32(" | ", 0xFFFFFFFF);
        TerminalPrintHex64Compact(fs.Files[i].Size);
        TerminalNewLine32(0x00000000);
    }
}
