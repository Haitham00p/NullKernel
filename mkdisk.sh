#!/usr/bin/env bash
# mkdisk.sh — prepare a raw .img as a NullOS install target.
# Usage: ./mkdisk.sh <image.img> [sizeMiB]
#   - creates the image if it does not exist (default 64 MiB)
#   - writes an MBR with one bootable FAT32 (0x0C) partition at LBA 2048
#   - formats that partition with FAT32, label NULLOS
set -euo pipefail

IMG="${1:?usage: $0 <image.img> [sizeMiB]}"
SIZE_M="${2:-64}"
SECTOR_START=2048

if [ ! -f "$IMG" ]; then
    echo "[*] creating $IMG (${SIZE_M} MiB)"
    truncate -s "${SIZE_M}M" "$IMG"
else
    echo "[*] reusing existing $IMG"
fi

echo "[*] wiping old partition table"
sfdisk --delete "$IMG" >/dev/null 2>&1 || true

echo "[*] writing MBR with FAT32 (0x0C) partition at sector ${SECTOR_START}"
printf 'label: dos\nunit: sectors\nstart=%d, type=0c, bootable\n' \
    "$SECTOR_START" | sfdisk "$IMG" >/dev/null

echo "[*] clearing gap before partition"
dd if=/dev/zero of="$IMG" bs=512 seek=1 count=$((SECTOR_START - 1)) \
    conv=notrunc status=none

echo "[*] formatting FAT32 (label NULLOS)"
mkfs.vfat -F 32 -n NULLOS --offset "$SECTOR_START" "$IMG"

echo "[*] done: $IMG ready"
sfdisk -l "$IMG" | sed -n '/Device/,$p'
