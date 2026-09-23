#!/usr/bin/env bash
# sync_src_to_rootfs.sh — copy every source folder into rootfs/src so the
# running OS has the full source tree available (incl. arch). Binaries and
# build artifacts are excluded to keep the disk image small.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
DEST="$ROOT/rootfs/src"

rm -rf "$DEST"
mkdir -p "$DEST"

for d in "$ROOT"/*/; do
    name="$(basename "$d")"
    case "$name" in
        build|.git|limine-binary|rootfs)
            continue ;;
    esac
    if [ -d "$d" ]; then
        cp -r "$d" "$DEST/"
    fi
done

echo "Synced source folders into $DEST"
