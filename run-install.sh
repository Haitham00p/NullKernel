#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

IMG="${1:-nullos.img}"
LIMINE_OUT="${2:-build/limine/out}"
PARTITION_START=2048
KERNEL="$SCRIPT_DIR/build/kernel.elf"
FAT_IMAGE="$IMG@@$((PARTITION_START * 512))"

if [[ -b "$IMG" ]]; then
    echo "error: refusing to install onto a physical block device; use a regular image file" >&2
    exit 1
fi

# Recreate the partition table and FAT32 ESP every install. This prevents an
# older NullL type-0x0C image from being mistaken for the new Limine ESP.
./mkdisk.sh "$IMG"

for tool in mcopy mmd mdir; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "error: $tool from mtools is required" >&2
        exit 1
    }
done

[[ -f "$KERNEL" ]] || { echo "error: $KERNEL missing; run make" >&2; exit 1; }
[[ -x "$LIMINE_OUT/limine" ]] || { echo "error: Limine host tool missing; run make limine" >&2; exit 1; }
[[ -f "$LIMINE_OUT/limine-bios.sys" ]] || { echo "error: limine-bios.sys missing" >&2; exit 1; }
[[ -f "$LIMINE_OUT/BOOTX64.EFI" ]] || { echo "error: BOOTX64.EFI missing" >&2; exit 1; }

echo "[*] copying NullOS root filesystem"
for item in "$SCRIPT_DIR"/rootfs/*; do
    [[ -e "$item" ]] || continue
    mcopy -s -o -i "$FAT_IMAGE" "$item" ::/
done

ensure_dir() {
    local path="$1"
    if ! mdir -i "$FAT_IMAGE" "$path" >/dev/null 2>&1; then
        mmd -i "$FAT_IMAGE" "$path"
    fi
}

ensure_dir ::/BOOT
ensure_dir ::/BOOT/LIMINE
ensure_dir ::/EFI
ensure_dir ::/EFI/BOOT

echo "[*] installing kernel and Limine configuration"
mcopy -o -i "$FAT_IMAGE" "$KERNEL" ::/BOOT/KERNEL.ELF
mcopy -o -i "$FAT_IMAGE" "$SCRIPT_DIR/boot/limine.conf" ::/BOOT/LIMINE/LIMINE.CONF
mcopy -o -i "$FAT_IMAGE" "$SCRIPT_DIR/boot/limine.conf" ::/EFI/BOOT/LIMINE.CONF
mcopy -o -i "$FAT_IMAGE" "$LIMINE_OUT/limine-bios.sys" ::/BOOT/LIMINE/LIMINE-BIOS.SYS
mcopy -o -i "$FAT_IMAGE" "$LIMINE_OUT/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI

echo "[*] installing Limine BIOS boot code"
"$LIMINE_OUT/limine" bios-install "$IMG"

echo "[*] Limine installed: $IMG (BIOS + UEFI x86-64)"
