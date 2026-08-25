#ifndef NLD_H
#define NLD_H

/* nld — the NullOS linker core.
 *
 * Combines one or more relocatable objects:
 *   - N64O objects produced by n64a (assembly)
 *   - N64O objects produced by NCC in object mode (C)
 * together with the kernel symbol vault into a final ELF executable
 * using exactly the image layout NccBuildElf() has always produced:
 *
 *   [0x000 .. 0x200)   ELF header + program headers (+ zero padding)
 *   [0x200 .. )        data of every object (16-byte aligned joins)
 *                      followed by text of every object
 *   single RWX PT_LOAD at 0x100200, entry = main
 *
 * The core is freestanding so the very same code runs on the host (CLI +
 * tests) and inside NullOS (self-hosted mixed builds).
 */

#include "../n64a/n64a_port.h"
#include <stdint.h>
#include <stdbool.h>

/* Image placement constants — must match ncc/ncc.c. */
#define NLD_IMAGE_BASE     0x100000ULL
#define NLD_IMAGE_HDRSZ    0x200ULL
#define NLD_ALIGN          16U

typedef struct
{
    const char  *Name;    /* object file name (for diagnostics)         */
    const void  *Data;
    uint32_t     Size;
} NldObject;

typedef struct
{
    const char *Name;
    uint64_t    Addr;
} NldSymbol;

typedef struct
{
    bool Failed;
    char Message[192];
} NldDiag;

/* Links the objects and writes a fresh ELF image.
 *
 * Vault semantics match NCC's NccResolveName(): definitions inside the
 * linked objects win over vault entries.  The caller owns OutElf (port
 * allocator). Returns false and fills Diag on failure. */
bool NldLink(const NldObject *Objects, uint32_t Count,
             const NldSymbol *Vault, uint32_t VaultCount,
             uint8_t **OutElf, uint32_t *OutElfSize,
             NldDiag *Diag);

#endif /* NLD_H */
