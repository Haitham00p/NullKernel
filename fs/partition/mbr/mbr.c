#include "mbr.h"
#include <stddef.h>

bool MBRIsValid(
    const MBR *Table
){
    return Table->Signature == 0xAA55;
}

const MBRPartitionEntry *
MBRGetPartition(
    const MBR *Table,
    uint8_t Index
){
    if (Index >= 4){
        return NULL;
    }
    return &Table->Partitions[Index];
}

uint8_t MBRGetPartitionCount(
    const MBR *Table
){
    uint8_t count = 0;
    for (uint32_t i = 0; i < 4; i++){
        if (Table->Partitions[i].PartitionType != 0){
            count++;
        }
    }
    return count;
}