/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

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