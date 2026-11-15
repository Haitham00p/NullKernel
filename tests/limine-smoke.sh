#!/usr/bin/env bash
set -euo pipefail

IMG="${1:?usage: bash tests/limine-smoke.sh <disk-image>}"
LOG="${TMPDIR:-/tmp}/nullos-limine-smoke-serial.log"
QEMU="${QEMU:-qemu-system-x86_64}"

rm -f "$LOG"

set +e
timeout 240s "$QEMU" \
    -machine pc \
    -cpu qemu64 \
    -m 1024 \
    -boot order=c \
    -drive "file=$IMG,format=raw,if=ide,index=0" \
    -display none \
    -serial "file:$LOG" \
    -no-reboot -no-shutdown
status=$?
set -e

# timeout(1) returns 124 when the guest correctly reaches its idle shell.
if [[ $status -ne 124 && $status -ne 0 ]]; then
    echo "FAIL: QEMU exited with status $status" >&2
    cat "$LOG" >&2 || true
    exit 1
fi

if ! grep -q "NullOS: kernel_main entered" "$LOG"; then
    echo "FAIL: Limine did not hand off to kernel_main" >&2
    cat "$LOG" >&2 || true
    exit 1
fi

if ! grep -q "NullOS: shell ready" "$LOG"; then
    echo "FAIL: kernel did not reach the normal shell" >&2
    cat "$LOG" >&2 || true
    exit 1
fi

echo "--- Limine BIOS serial trace ---"
cat "$LOG"
echo "PASS: Limine BIOS -> NullBootInfo adapter -> kernel -> Normal shell"
