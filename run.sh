#!/usr/bin/env bash
# run.sh — build the kernel, deploy the ISO, boot from ISO only.
set -euo pipefail

cd "$(dirname "$0")"

echo "[*] building..."
make -s -j8

echo "[*] booting from ISO..."
exec ./start.sh
