#!/usr/bin/env bash
set -euo pipefail

IMG="${1:?usage: bash tests/nulll-smoke.sh <disk-image>}"
LOG=/tmp/nulll-smoke-serial.log
MON=/tmp/nulll-smoke-monitor.sock
ENTRY_PHYS=$(python3 - <<'PY'
import struct
with open("build/kernel.elf", "rb") as f:
    image = f.read(64)
entry = struct.unpack_from("<Q", image, 24)[0]
print(hex(entry - 0xffffffff80000000))
PY
)

rm -f "$LOG" "$MON"

qemu-system-x86_64 \
    -m 1024 \
    -boot order=c \
    -drive "file=$IMG,format=raw,if=ide,index=0" \
    -display none \
    -serial "file:$LOG" \
    -monitor "unix:$MON,server=on,wait=off" \
    -no-reboot -no-shutdown \
    -name NullL-smoke &
QEMU_PID=$!

cleanup() {
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$MON"
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*" >&2
    if [[ -S "$MON" ]]; then
        echo "--- QEMU CPU state / page tables ---" >&2
        python3 - "$MON" "$ENTRY_PHYS" <<'PY' >&2 || true
import socket
import sys
import time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(0.25)
try:
    sock.connect(sys.argv[1])
    try:
        sock.recv(4096)
    except OSError:
        pass
    commands = (
        "info registers\ninfo mem\n"
        "xp /4gx 0x11000\nxp /4gx 0x12000\n"
        "xp /8gx 0x13000\nxp /1gx 0x12ff8\n"
        "xp /4gx 0x6000\n"
        "xp /4gx 0x6e00\n"
        "xp /8gx 0x100000\n"
        "xp /8gx 0x59900\n"
        f"xp /8gx {sys.argv[2]}\n"
        "xp /32gx 0x55220\nxp /32gx 0x144220\n"
        "xp /4gx 0x172060\nxp /2gx 0x176240\n"
        "xp /2gx 0x176360\nxp /8gx 0x37d7e0\n"
    )
    sock.sendall(commands.encode())
    chunks = []
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        try:
            data = sock.recv(8192)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
    print(b"".join(chunks).decode("utf-8", "replace"))
except OSError as exc:
    print(f"QEMU monitor unavailable: {exc}")
finally:
    sock.close()
PY
    fi
    echo "--- NullL serial trace ---" >&2
    cat "$LOG" 2>/dev/null || true
    exit 1
}

# Wait until Stage 2 proves it reached the interactive menu.
for _ in $(seq 1 150); do
    grep -q "NullL: menu displayed; waiting for key" "$LOG" 2>/dev/null && break
    kill -0 "$QEMU_PID" 2>/dev/null || fail "QEMU exited before the menu"
    sleep 0.1
done
grep -q "NullL: menu displayed; waiting for key" "$LOG" 2>/dev/null ||
    fail "Stage 2 never reached the menu"

# Inject a real guest key through QEMU's monitor rather than relying on a GUI.
python3 - "$MON" <<'PY'
import socket
import sys
import time

path = sys.argv[1]
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1.0)
for _ in range(50):
    try:
        sock.connect(path)
        break
    except OSError:
        time.sleep(0.1)
else:
    raise SystemExit("QEMU monitor socket did not become ready")

try:
    try:
        sock.recv(4096)  # HMP greeting/prompt
    except (TimeoutError, OSError):
        pass
    sock.sendall(b"sendkey 1\n")
    time.sleep(0.2)
finally:
    sock.close()
PY

for _ in $(seq 1 100); do
    grep -q "NullL: key=1" "$LOG" 2>/dev/null && break
    sleep 0.1
done
grep -q "NullL: key=1" "$LOG" 2>/dev/null ||
    fail "QEMU sendkey reached no NullL key event"

grep -q "NullL: selected mode 1" "$LOG" 2>/dev/null ||
    fail "NullL received key 1 but did not select Normal-Mode"

for _ in $(seq 1 150); do
    grep -q "NullL native kernel entry" "$LOG" 2>/dev/null && break
    grep -q "NullL: fatal disk error" "$LOG" 2>/dev/null &&
        fail "Stage 2 reported a BIOS disk error"
    grep -q "NullL: fatal kernel load/ELF error" "$LOG" 2>/dev/null &&
        fail "Stage 2 could not load/parse the kernel ELF"
    grep -q "NullL: fatal E820 memory map error" "$LOG" 2>/dev/null &&
        fail "Stage 2 could not collect the E820 memory map"
    sleep 0.1
done
grep -q "NullL native kernel entry" "$LOG" 2>/dev/null ||
    fail "selected mode 1 did not reach kernel_main"

for _ in $(seq 1 300); do
    grep -q "NullOS: shell ready" "$LOG" 2>/dev/null && break
    grep -q "=== NULLCONSOLE FAULT REPORT ===" "$LOG" 2>/dev/null &&
        fail "kernel exception occurred before shell initialization"
    sleep 0.1
done
grep -q "NullOS: shell ready" "$LOG" 2>/dev/null ||
    fail "kernel did not initialize the shell"

# Exercise the PS/2 -> IRQ1 -> keyboard decoder -> shell parser/dispatcher path.
python3 - "$MON" <<'PY'
import socket
import sys
import time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(1.0)
sock.connect(sys.argv[1])
try:
    try:
        sock.recv(4096)
    except (TimeoutError, OSError):
        pass
    for key in ("h", "e", "l", "p", "ret"):
        sock.sendall(f"sendkey {key}\\n".encode())
        time.sleep(0.08)
finally:
    sock.close()
PY

for _ in $(seq 1 150); do
    grep -q "NullOS: shell command dispatched" "$LOG" 2>/dev/null && break
    sleep 0.1
done
grep -q "NullOS: shell command dispatched" "$LOG" 2>/dev/null ||
    fail "shell did not dispatch the injected help command"

echo "--- NullOS serial trace ---"
cat "$LOG"
echo "PASS: BIOS -> NullL menu -> ELF -> kernel -> shell -> keyboard -> help dispatch"
