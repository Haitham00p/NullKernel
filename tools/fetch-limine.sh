#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:?usage: fetch-limine.sh <output-directory>}"
VERSION="12.9.6"
ARCHIVE_SHA256="b38aabb042b0c0f9f562b7741be3c1204838d7bbacfa78bb411c674bd5b984ee"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="$ROOT_DIR/build/limine-cache"
SOURCE_DIR="$CACHE_DIR/source"
ARCHIVE="$CACHE_DIR/limine-binary.tar.gz"

mkdir -p "$CACHE_DIR" "$OUT_DIR"

if [[ ! -f "$ARCHIVE" ]]; then
    curl --fail --location --retry 3 --output "$ARCHIVE" \
        "https://github.com/limine-bootloader/limine/releases/download/v$VERSION/limine-binary.tar.gz"
fi

echo "$ARCHIVE_SHA256  $ARCHIVE" | sha256sum --check --status || {
    echo "error: Limine v$VERSION archive checksum mismatch" >&2
    rm -f "$ARCHIVE"
    exit 1
}

if [[ ! -d "$SOURCE_DIR" ]]; then
    mkdir -p "$SOURCE_DIR"
    tar -xzf "$ARCHIVE" -C "$SOURCE_DIR"
fi

MAKEFILE="$(find "$SOURCE_DIR" -type f \\( -name Makefile -o -name GNUmakefile \\) -print -quit)"
if [[ -n "$MAKEFILE" ]]; then
    make -C "$(dirname "$MAKEFILE")"
fi

find_one() {
    local name="$1"
    find "$SOURCE_DIR" -type f -name "$name" -print -quit
}

HOST_TOOL="$(find "$SOURCE_DIR" -type f -name limine -perm /111 -print -quit)"
BIOS_SYS="$(find_one limine-bios.sys)"
EFI_APP="$(find_one BOOTX64.EFI)"

for item in "$HOST_TOOL" "$BIOS_SYS" "$EFI_APP"; do
    if [[ -z "$item" || ! -f "$item" ]]; then
        echo "error: Limine archive is missing a required BIOS/UEFI file" >&2
        exit 1
    fi
done

cp "$HOST_TOOL" "$OUT_DIR/limine"
cp "$BIOS_SYS" "$OUT_DIR/limine-bios.sys"
cp "$EFI_APP" "$OUT_DIR/BOOTX64.EFI"
chmod +x "$OUT_DIR/limine"
echo "Limine v$VERSION ready in $OUT_DIR"
