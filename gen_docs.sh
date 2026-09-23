#!/usr/bin/env bash
# gen_docs.sh — turn every header in the source tree into a .txt help file.
# Output: DOCS/<relpath>.txt (and a mirror in rootfs/docs so the OS has them).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/DOCS"
SRC_DOC="$ROOT/rootfs/docs"

rm -rf "$OUT"
mkdir -p "$OUT"
mkdir -p "$SRC_DOC"
rm -rf "$SRC_DOC"/*

count=0
while IFS= read -r f; do
    rel="${f#$ROOT/}"
    name="${rel//\//_}"
    name="${name%.h}.txt"
    {
        echo "NullOS Header Help"
        echo "=================="
        echo "File: $rel"
        echo ""
        echo "----------------------------------------"
        cat "$f"
    } > "$OUT/$name"
    cp "$OUT/$name" "$SRC_DOC/$name"
    count=$((count + 1))
done < <(find "$ROOT" -name '*.h' -type f \
            -not -path '*/build/*' \
            -not -path '*/.git/*' \
            -not -path '*/limine-binary/*' \
            | sort)

echo "Generated $count header help files in $OUT"
echo "Mirrored to $SRC_DOC"
