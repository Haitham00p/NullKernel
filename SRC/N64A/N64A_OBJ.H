#ifndef N64A_OBJ_H
#define N64A_OBJ_H

#include "n64a_asm.h"

/* Serializes the assembled state into a N64O object blob.
 * The caller receives a buffer allocated with the port allocator. */
bool N64aWriteObject(N64aAsm *a, uint8_t **OutObject,
                     uint32_t *OutObjectSize);

#endif
