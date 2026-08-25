#!/bin/bash
cd /home/null/Kernel
pkill -9 -f qemu-system-x86_64 2>/dev/null; sleep 1
rm -f /tmp/nullos-mon.sock /tmp/serial-disk.log .test/v*.ppm
setsid qemu-system-x86_64 -m 256 -boot order=c -hda test2.img \
  -serial file:/tmp/serial-disk.log -display none \
  -monitor unix:/tmp/nullos-mon.sock,server,nowait \
  </dev/null >/dev/null 2>&1 &

shot() { python3 /tmp/opencode/qmon.py shot; mv /tmp/nullos-shot.ppm .test/v$1.ppm 2>/dev/null; }

sleep 12; shot A
python3 /tmp/opencode/qmon.py key l; python3 /tmp/opencode/qmon.py key s; python3 /tmp/opencode/qmon.py key ret
sleep 3; shot B
python3 /tmp/opencode/qmon.py key h; python3 /tmp/opencode/qmon.py key e; python3 /tmp/opencode/qmon.py key l; python3 /tmp/opencode/qmon.py key l; python3 /tmp/opencode/qmon.py key o; python3 /tmp/opencode/qmon.py key ret
sleep 3; shot C
python3 /tmp/opencode/qmon.py key t; python3 /tmp/opencode/qmon.py key i; python3 /tmp/opencode/qmon.py key c; python3 /tmp/opencode/qmon.py key k; python3 /tmp/opencode/qmon.py key s; python3 /tmp/opencode/qmon.py key ret
sleep 3; shot D
python3 /tmp/opencode/qmon.py key t; python3 /tmp/opencode/qmon.py key i; python3 /tmp/opencode/qmon.py key m; python3 /tmp/opencode/qmon.py key e; python3 /tmp/opencode/qmon.py key ret
sleep 3; shot E
pkill -9 -f qemu-system-x86_64 2>/dev/null
ls -la .test/v*.ppm