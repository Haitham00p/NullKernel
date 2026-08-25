#!/usr/bin/env python3
import sys

ISO = "/home/null/Kernel/build/NullOS.iso"

def sec(f, n):
    f.seek(n * 2048)
    return f.read(2048)

def le32(b, off):
    return int.from_bytes(b[off:off+4], "little")

def parse_record(buf, off):
    rl = buf[off]
    if rl == 0:
        return None
    lba = le32(buf, off + 2)
    size = le32(buf, off + 10)
    flags = buf[off + 25]
    nlen = buf[off + 32]
    name = buf[off + 33:off + 33 + nlen]
    return {"off": off, "rl": rl, "lba": lba, "size": size, "flags": flags, "name": name}

def walk(f, lba, size, depth=0, maxdump=200):
    if depth > 6 or size == 0:
        return
    nsec = (size + 2047) // 2048
    print(f"{'  '*depth}[dir] lba={lba} size={size} sectors={nsec}")
    off = 0
    for s in range(nsec):
        buf = sec(f, lba + s)
        o = 0
        while o < 2048:
            rec = parse_record(buf, o)
            if rec is None:
                if o > 1800 and s == 0:
                    tail = buf[o:2048]
                    print(f"{'  '*depth}  endmark at off {o}; next 16 bytes: {tail[:16].hex(' ')}")
                break
            skip = rec["name"] in (b"\x00", b"\x01")
            print(f"{'  '*depth}  off={rec['off']:4d} rl={rec['rl']:3d} lba={rec['lba']:5d} size={rec['size']:5d} f={rec['flags']:02x} {'SKIP' if skip else rec['name'].decode('latin1')}")
            o += rec["rl"]
            if skip:
                continue
            if rec["flags"] & 0x02:
                walk(f, rec["lba"], rec["size"], depth + 1)
            off += rec["rl"]
        if o < 2048:
            break

with open(ISO, "rb") as f:
    pvd = sec(f, 16)
    assert pvd[1:6] == b"CD001", "no PVD"
    root = parse_record(pvd, 156)
    print("root:", root)
    # find /rootfs
    buf = sec(f, root["lba"])
    o = 0
    rootfs = None
    while o < 2048:
        rec = parse_record(buf, o)
        if rec is None:
            break
        if rec["name"] == b"ROOTFS":
            rootfs = rec
        o += rec["rl"]
    print("rootfs:", rootfs)
    if rootfs:
        walk(f, rootfs["lba"], rootfs["size"])