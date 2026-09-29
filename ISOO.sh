#!/usr/bin/env bash
set -euo pipefail

make

exec qemu-system-x86_64 \
    -boot order=d \
    -cdrom build/NullOS.iso \
    -m 3 \
    -serial stdio \
    -no-reboot
