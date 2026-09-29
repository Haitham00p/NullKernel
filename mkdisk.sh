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

# -R 64 reserved sectors: the loader reads its mailbox (LBA 2050), manifest
# (LBA 2051) and 48-sector stage2 (LBA 2055..2102) from the head of the
# partition. The default FAT32 reserved count of 32 would place FAT1 at
# LBA 2080, so writing stage2 would clobber the allocation table and the
# kernel would later hand the already-allocated kernel.elf clusters back
# out to the filesystem. Keep FAT1 clear of the loader scratch area.
echo "[*] formatting FAT32 (label NULLOS, 64 reserved sectors)"
mkfs.vfat -F 32 -n NULLOS -R 64 --offset "$SECTOR_START" "$IMG"

echo "[*] done: $IMG ready"
sfdisk -l "$IMG" | sed -n '/Device/,$p'
