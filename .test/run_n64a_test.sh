#!/bin/bash
# .test/run_n64a_test.sh — boot the installed disk, wait for the full
# self-host build (incl. mixed C+ASM projects), then run MIXED and ASMDEMO.
cd /home/null/Kernel
pkill -9 -f qemu-system-x86_64 2>/dev/null; sleep 1
rm -f /tmp/nullos-mon.sock /tmp/serial-disk.log .test/n_*.ppm
setsid qemu-system-x86_64 -m 256 -boot order=c \
  -drive file=/tmp/opencode/n64test.img,format=raw,if=ide,index=0 \
  -serial file:/tmp/serial-disk.log -display none \
  -monitor unix:/tmp/nullos-mon.sock,server,nowait \
  </dev/null >/dev/null 2>&1 &

# poll serial log until shell prompt appears (max ~150s)
for i in $(seq 1 60); do
  sleep 2.5
  if grep -q "Developer@" /tmp/serial-disk.log 2>/dev/null; then break; fi
done
sleep 2
python3 .test/qmon.py shot >/dev/null 2>&1
mv /tmp/nullos-shot.ppm .test/n_boot1.ppm 2>/dev/null

type_cmd() {
  for k in $(seq 0 $(( ${#1} - 1 ))); do
    python3 .test/qmon.py key "${1:$k:1}"
    sleep 0.08
  done
  python3 .test/qmon.py key ret
}

type_cmd "mixed"; sleep 4
python3 .test/qmon.py shot >/dev/null 2>&1
mv /tmp/nullos-shot.ppm .test/n_mixed.ppm 2>/dev/null

type_cmd "asmdemo"; sleep 4
python3 .test/qmon.py shot >/dev/null 2>&1
mv /tmp/nullos-shot.ppm .test/n_asmdemo.ppm 2>/dev/null

pkill -9 -f qemu-system-x86_64 2>/dev/null
echo done
