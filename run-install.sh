#!/usr/bin/env bash
# run-install.sh — stage boot files on a disk image, install Limine, boot the
# NullOS live CD, and (optionally) auto-press 'I' in the VM to run the installer.
# Usage: ./run-install.sh [disk.img] [--auto]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

IMG="${1:-}"
AUTO=0
if [ "${2:-}" = "--auto" ] || [ "${1:-}" = "--auto" ]; then
    AUTO=1
    IMG="${1:-}"
    [ "$IMG" = "--auto" ] && IMG="${2:-}"
fi
if [ -z "$IMG" ]; then
    read -rp "Disk image file (e.g. mydisk.img): " IMG
fi
if [ -z "$IMG" ]; then
    echo "error: no disk image given" >&2
    exit 1
fi

KERNEL="$SCRIPT_DIR/build/kernel.elf"
LIMINE_SYS="$SCRIPT_DIR/limine-binary/limine-bios.sys"
LIMINE_CONF="$SCRIPT_DIR/iso/boot/limine/limine.conf"
LIMINE_BIN="$SCRIPT_DIR/limine-binary/limine"
ISO="$SCRIPT_DIR/build/NullOS.iso"

echo "[*] checking build products"
if [ ! -f "$KERNEL" ] || [ ! -f "$ISO" ]; then
    echo "[*] kernel.elf or NullOS.iso missing, running make"
    make -C "$SCRIPT_DIR" >/dev/null
fi
for f in "$KERNEL" "$LIMINE_SYS" "$LIMINE_CONF" "$ISO"; do
    if [ ! -f "$f" ]; then
        echo "error: $f not found" >&2
        exit 1
    fi
done

if [ ! -f "$IMG" ]; then
    echo "error: $IMG does not exist (create it with ./mkdisk.sh $IMG)" >&2
    exit 1
fi

PART_OFFSET=2048
IMG_OFFSET="$IMG"@@$((PART_OFFSET * 512))

echo "[*] staging boot files on $IMG (host side)"
mmd -i "$IMG_OFFSET" ::/BOOT ::/BOOT/LIMINE 2>/dev/null || true
mcopy -o -i "$IMG_OFFSET" "$KERNEL" ::/BOOT/KERNEL.ELF
mcopy -o -i "$IMG_OFFSET" "$LIMINE_SYS" ::/BOOT/LIMINE/limine-bios.sys
mcopy -o -i "$IMG_OFFSET" "$LIMINE_CONF" ::/BOOT/LIMINE/limine.conf

echo "[*] installing Limine BIOS bootloader on $IMG"
"$LIMINE_BIN" bios-install "$IMG"

if [ "$AUTO" = 1 ]; then
    echo "[*] booting NullOS live CD, disk: $IMG (auto-pressing I to install)"
    exec qemu-system-x86_64 -m 3 -boot order=d -cdrom "$ISO" \
        -hda "$IMG" -serial stdio -display none \
        -monitor unix:/tmp/nullos-mon.sock,server,nowait
fi

echo "[*] booting NullOS live CD, disk: $IMG (press I in the VM to install)"
exec qemu-system-x86_64 -m 3 -boot order=d -cdrom "$ISO" \
    -hda "$IMG" -serial stdio
