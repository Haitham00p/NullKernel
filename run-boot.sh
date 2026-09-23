#!/usr/bin/env bash
# run-boot.sh — boot NullOS installed on a disk image (no CD) with a
# graphical display so you can SEE and USE the mouse cursor.
#
# Usage:
#   ./run-boot.sh              # boots nullos.img in a GTK window
#   ./run-boot.sh mydisk.img   # boot a specific disk
#   ./run-boot.sh vnc         # expose a VNC server on :1 (connect with a
#                             #   VNC viewer to localhost:5901) for remote use
#   ./run-boot.sh vnc mydisk.img
set -euo pipefail

DISP="-display gtk"
IMG="nullos.img"

case "${1:-}" in
    vnc) DISP="-vnc :1"; IMG="${2:-nullos.img}" ;;
    "")  : ;;
    *)   IMG="$1" ;;
esac

if [ ! -f "$IMG" ]; then
    echo "error: $IMG does not exist (create it with ./mkdisk.sh $IMG)" >&2
    exit 1
fi

BACKEND="${DISP#-display }"
echo "[*] booting NullOS from disk: $IMG"
echo "[*] display backend: $BACKEND"
echo "[*] serial log: /tmp/nullos-serial.log"
echo "[*] click the window to capture the mouse; Ctrl+Alt releases it"
if [ "$BACKEND" = "vnc=:1" ]; then
    echo "[*] connect a VNC viewer to localhost:5901"
fi

exec qemu-system-x86_64 -m 1024 -boot order=c \
    -drive "file=$IMG,format=raw,if=ide,index=0" \
    $DISP \
    -serial file:/tmp/nullos-serial.log \
    -name "NullOS"
