#ifndef FAT32_DIRECTORY_H
#define FAT32_DIRECTORY_H

#include <stdbool.h>
#include <stdint.h>

#include "../vbr/vbr.h"

#define FAT32_DIRECTORY_ENTRY_SIZE 32U
#define FAT32_SHORT_NAME_LENGTH    11U
#define FAT32_SHORT_NAME_MAX       12U /* NAME.EXT plus NUL */

#define FAT32_ATTRIBUTE_READ_ONLY  0x01U
#define FAT32_ATTRIBUTE_HIDDEN     0x02U
#define FAT32_ATTRIBUTE_SYSTEM     0x04U
#define FAT32_ATTRIBUTE_VOLUME_ID  0x08U
#define FAT32_ATTRIBUTE_DIRECTORY  0x10U
#define FAT32_ATTRIBUTE_ARCHIVE    0x20U
#define FAT32_ATTRIBUTE_LONG_NAME  0x0FU

#define FAT32_DIRECTORY_ENTRY_FREE    0x00U
#define FAT32_DIRECTORY_ENTRY_DELETED 0xE5U

/* The 32-byte short directory entry layout stored on a FAT32 volume. */
typedef struct __attribute__((packed))
{
    uint8_t Name[FAT32_SHORT_NAME_LENGTH];
    uint8_t Attributes;
    uint8_t NTReserved;
    uint8_t CreationTimeTenths;
    uint16_t CreationTime;
    uint16_t CreationDate;
    uint16_t LastAccessDate;
    uint16_t FirstClusterHigh;
    uint16_t WriteTime;
    uint16_t WriteDate;
    uint16_t FirstClusterLow;
    uint32_t FileSize;
} FAT32DirectoryEntry;

_Static_assert(sizeof(FAT32DirectoryEntry) == FAT32_DIRECTORY_ENTRY_SIZE,
               "A FAT32 directory entry must occupy exactly 32 bytes");

typedef struct
{
    const FAT32BootSector *Boot;
    uint32_t PartitionStartLBA;
    uint32_t FirstCluster;
} FAT32Directory;

typedef bool (*FAT32DirectoryVisitor)(const FAT32DirectoryEntry *Entry,
                                      const char *ShortName,
                                      void *Context);

bool FAT32DirectoryOpenRoot(FAT32Directory *Directory,
                            const FAT32BootSector *Boot,
                            uint32_t PartitionStartLBA);

bool FAT32DirectoryOpen(FAT32Directory *Directory,
                        const FAT32BootSector *Boot,
                        uint32_t PartitionStartLBA,
                        uint32_t FirstCluster);

/* Converts a user name to an uppercase FAT 8.3 name. */
bool FAT32DirectoryEncodeShortName(const char *Name,
                                   uint8_t Encoded[FAT32_SHORT_NAME_LENGTH]);

void FAT32DirectoryDecodeShortName(const FAT32DirectoryEntry *Entry,
                                   char Output[FAT32_SHORT_NAME_MAX + 1U]);

uint32_t FAT32DirectoryEntryFirstCluster(const FAT32DirectoryEntry *Entry);

bool FAT32DirectoryForEach(const FAT32Directory *Directory,
                           FAT32DirectoryVisitor Visitor, void *Context);

bool FAT32DirectoryFind(const FAT32Directory *Directory, const char *Name,
                        FAT32DirectoryEntry *Entry);

/* Creates a short-name file or directory entry in Directory. */
bool FAT32DirectoryCreateEntry(FAT32Directory *Directory, const char *Name,
                               uint8_t Attributes, uint32_t FirstCluster,
                               uint32_t FileSize, FAT32DirectoryEntry *Entry);

/* Allocates and initializes a new empty subdirectory, including . and ... */
bool FAT32DirectoryCreateDirectory(FAT32Directory *Parent, const char *Name,
                                   FAT32Directory *CreatedDirectory);

#endif
