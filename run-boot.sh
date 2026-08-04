#!/usr/bin/env bash
# run-boot.sh — boot NullOS installed on a disk image (no CD).
# Usage: ./run-boot.sh [disk.img]
set -euo pipefail

IMG="${1:-}"
if [ -z "$IMG" ]; then
    read -rp "Disk image file (e.g. mydisk.img): " IMG
fi
if [ -z "$IMG" ]; then
    echo "error: no disk image given" >&2
    exit 1
fi
if [ ! -f "$IMG" ]; then
    echo "error: $IMG does not exist (create it with ./mkdisk.sh $IMG)" >&2
    exit 1
fi

echo "[*] booting NullOS from disk: $IMG"
exec qemu-system-x86_64 -m 3 -boot order=c \
    -drive "file=$IMG,format=raw,if=ide,index=0" -serial stdio -machine pcspk-audiodev=speaker -audiodev driver=pipewire,id=speaker