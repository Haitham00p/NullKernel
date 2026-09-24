#!/usr/bin/env bash
# Generate complete header/API help for the source tree.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/DOCS"
SRC_DOC="$ROOT/rootfs/DOCS"

rm -rf "$OUT"
mkdir -p "$OUT" "$SRC_DOC"

count=0
while IFS= read -r f; do
    rel="${f#$ROOT/}"
    name="${rel//\//_}"
    name="${name%.h}.txt"
    {
        echo "NullOS Header/API Help"
        echo "======================"
        echo "Header: $rel"
        echo ""
        echo "This file is generated from the authoritative header."
        echo "See subsystem documents in rootfs/DOCS for architecture and usage."
        echo ""
        echo "----------------------------------------"
        cat "$f"
    } > "$OUT/$name"
    cp "$OUT/$name" "$SRC_DOC/$name"
    count=$((count + 1))
done < <(find "$ROOT" -name "*.h" -type f \
    -not -path "*/build/*" \
    -not -path "*/.git/*" \
    -not -path "*/limine-binary/*" \
    | sort)

echo "Generated $count header/API files in $OUT and rootfs/DOCS"
