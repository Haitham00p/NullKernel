#!/usr/bin/env bash
set -euo pipefail

exec qemu-system-x86_64 \
    -boot order=d \
    -cdrom build/NullOS.iso \
    -hda esp.img \
    -serial stdio \
    -m 3 \
    -no-reboot
