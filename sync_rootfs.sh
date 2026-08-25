#!/usr/bin/env bash
# sync_rootfs.sh — pull the running OS rootfs back into the source tree.
#
# The OS is self-hosting: at boot it recompiles rootfs/bin/*.C into *.EXE on the
# disk. This script copies the disk's rootfs into ./rootfs so those updates (and
# any edited registry / data files) land back in the source tree.
#
# The on-disk FAT volume uses UPPERCASE names (BIN, ETC, REG, ...). The source
# tree uses lowercase dirs (bin, etc, reg, ...). To avoid creating duplicate
# directories, each disk entry is merged into its lowercased destination:
#   disk:/BIN   -> ./rootfs/bin   (contents merged, not replaced)
#   disk:/REG   -> ./rootfs/reg   (registry defaults)
#   disk:/HELLO.TXT -> ./rootfs/HELLO.TXT (same case, overwritten)
#
# Usage: ./sync_rootfs.sh [disk.img]
#   default image: /tmp/cdtest.img
#   the bootloader directory /BOOT is excluded (build artifact, not source rootfs).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

IMG="${1:-/tmp/cdtest.img}"
if [ ! -f "$IMG" ]; then
    echo "error: $IMG not found" >&2
    exit 1
fi

PART_OFFSET=$((2048 * 512))
OFF="$IMG"@@"$PART_OFFSET"
DEST="$SCRIPT_DIR/rootfs"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "[*] staging disk rootfs into temp: $IMG"
mkdir -p "$DEST"

# Bare listing of disk root: "::/NAME" for files, "::/NAME/" for directories.
mapfile -t ENTRIES < <(mdir -b -i "$OFF" ::/ 2>/dev/null | sed 's#^::/##')

for E in "${ENTRIES[@]}"; do
    [ -z "$E" ] && continue
    NAME="${E%/}"
    [ -z "$NAME" ] && continue
    if [ "${NAME^^}" = "BOOT" ]; then
        echo "  skip  /BOOT"
        continue
    fi
    if [[ "$E" == */ ]]; then
        echo "  stage /$NAME/ (recursive)"
        mcopy -s -o -i "$OFF" "::/$NAME" "$TMP/" >/dev/null
    else
        echo "  stage /$NAME"
        mcopy -o -i "$OFF" "::/$NAME" "$TMP/" >/dev/null
    fi
done

echo "[*] merging into $DEST"
for N in "$TMP"/*; do
    [ -e "$N" ] || continue
    base="$(basename "$N")"
    if [ -d "$N" ]; then
        lc="$(echo "$base" | tr 'A-Z' 'a-z')"
        # Reuse an existing destination dir case-insensitively so we do not
        # create a duplicate (e.g. disk "DOCS" vs source "DOCS"/"docs").
        if [ -d "$DEST/$base" ]; then
            target="$DEST/$base"
        elif [ -d "$DEST/$lc" ]; then
            target="$DEST/$lc"
        else
            target="$DEST/$lc"
        fi
        echo "  merge /$base/ -> $(basename "$target")/"
        mkdir -p "$target"
        cp -r -f "$N/." "$target/"
    else
        echo "  merge /$base"
        cp -f "$N" "$DEST/$base"
    fi
done

echo "[*] done. source rootfs updated (disk rootfs merged, no duplicate dirs)."
