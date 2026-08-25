#!/bin/bash
cd /home/null/Kernel
pkill -9 -f qemu-system-x86_64 2>/dev/null; sleep 1
rm -f /tmp/serial-disk.log
setsid qemu-system-x86_64 -m 256 -boot order=c -hda test2.img \
  -serial file:/tmp/serial-disk.log -display none \
  -monitor unix:/tmp/nullos-mon.sock,server,nowait \
  </dev/null >/dev/null 2>&1 &
sleep 20
echo "=== serial log:"
cat /tmp/serial-disk.log
echo "=== done"
pkill -9 -f qemu-system-x86_64 2>/dev/null
