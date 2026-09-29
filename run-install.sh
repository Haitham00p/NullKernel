#!/usr/bin/env bash
# Native NullL disk installer. No third-party bootloader or chainloading.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
IMG="${1:-nullos.img}"
PARTITION_START=2048
STAGE2_OFFSET=7
MAILBOX_OFFSET=2
MANIFEST_OFFSET=3

if [ ! -f "$IMG" ]; then ./mkdisk.sh "$IMG"; fi
command -v mcopy >/dev/null 2>&1 || { echo "error: mtools is required" >&2; exit 1; }
command -v mmd >/dev/null 2>&1 || { echo "error: mtools is required" >&2; exit 1; }
command -v mrd >/dev/null 2>&1 || { echo "error: mtools (mrd) is required" >&2; exit 1; }

KERNEL="$SCRIPT_DIR/build/kernel.elf"
STAGE1="$SCRIPT_DIR/build/nulll/stage1.bin"
STAGE2="$SCRIPT_DIR/build/nulll/stage2.bin"
[ -f "$KERNEL" ] || { echo "error: $KERNEL missing; run make" >&2; exit 1; }
[ -f "$STAGE1" ] || { echo "error: $STAGE1 missing; run make nulll" >&2; exit 1; }
[ -f "$STAGE2" ] || { echo "error: $STAGE2 missing; run make nulll" >&2; exit 1; }

echo "[*] staging NullOS FAT32 filesystem"
# Do not invoke mtools here.  The installer writes the already-formatted FAT32
# partition directly, avoiding mtools locking/interactive probing entirely.
python3 - "$IMG" "$KERNEL" <<'PY'
import os, struct, sys

img_path, kernel_path = sys.argv[1], sys.argv[2]
PART = 2048
SECTOR = 512
part_off = PART * SECTOR

with open(img_path, "r+b") as f:
    f.seek(part_off)
    bpb = f.read(SECTOR)
    if len(bpb) != SECTOR:
        raise SystemExit("cannot read FAT32 boot sector")

    bps = struct.unpack_from("<H", bpb, 11)[0]
    spc = bpb[13]
    reserved = struct.unpack_from("<H", bpb, 14)[0]
    fats = bpb[16]
    fat_size = struct.unpack_from("<I", bpb, 36)[0]
    root = struct.unpack_from("<I", bpb, 44)[0]
    total = struct.unpack_from("<I", bpb, 32)[0]

    if bps != 512 or not spc or not fats or not fat_size or root < 2:
        raise SystemExit("invalid FAT32 BPB")

    fat_base = part_off + reserved * bps
    data_lba = PART + reserved + fats * fat_size
    data_off = data_lba * bps
    cluster_bytes = spc * bps
    cluster_count = (total - (reserved + fats * fat_size)) // spc

    def fat_get(c):
        f.seek(fat_base + c * 4)
        return struct.unpack("<I", f.read(4))[0] & 0x0fffffff

    def fat_set(c, v):
        for n in range(fats):
            f.seek(fat_base + n * fat_size * bps + c * 4)
            f.write(struct.pack("<I", v | 0x0f000000))

    def cluster_off(c):
        return data_off + (c - 2) * cluster_bytes

    def read_cluster(c):
        f.seek(cluster_off(c))
        return bytearray(f.read(cluster_bytes))

    def write_cluster(c, data):
        if len(data) != cluster_bytes:
            raise SystemExit("internal FAT32 cluster size error")
        f.seek(cluster_off(c))
        f.write(data)

    def short_name(name):
        base, dot, ext = name.upper().partition(".")
        return (base[:8].ljust(8) + ext[:3].ljust(3)).encode("ascii")

    def entries_in_chain(start):
        c, seen = start, set()
        while 2 <= c < 0x0ffffff8 and c not in seen:
            seen.add(c)
            data = read_cluster(c)
            for off in range(0, cluster_bytes, 32):
                e = data[off:off+32]
                if e[0] == 0x00:
                    return
                if e[0] == 0xe5 or e[11] == 0x0f:
                    continue
                yield c, off, e
            c = fat_get(c)

    def find_short(start, wanted):
        for c, off, e in entries_in_chain(start):
            if e[:11] == wanted:
                return c, off, e
        return None

    def alloc_clusters(count):
        free = []
        for c in range(2, cluster_count + 2):
            if fat_get(c) == 0:
                free.append(c)
                if len(free) == count:
                    break
        if len(free) != count:
            raise SystemExit("not enough free FAT32 clusters")
        for i, c in enumerate(free):
            fat_set(c, 0x0fffffff if i + 1 == len(free) else free[i + 1])
        return free

    def make_dir(parent, name):
        hit = find_short(parent, short_name(name))
        if hit:
            e = hit[2]
            return (e[20] << 16) | (e[26] | (e[21] << 8 if False else 0))

        clusters = alloc_clusters(1)
        c = clusters[0]
        data = bytearray(cluster_bytes)
        # . and ..
        data[0:11] = b".          "
        data[11] = 0x10
        data[20:22] = struct.pack("<H", c >> 16)
        data[26:28] = struct.pack("<H", c & 0xffff)
        data[32:43] = b"..         "
        data[43] = 0x10
        data[52:54] = struct.pack("<H", parent >> 16)
        data[58:60] = struct.pack("<H", parent & 0xffff)
        write_cluster(c, data)

        p = read_cluster(parent)
        slot = None
        for off in range(0, cluster_bytes, 32):
            if p[off] in (0x00, 0xe5):
                slot = off
                break
        if slot is None:
            raise SystemExit("directory cluster full")
        e = bytearray(32)
        e[:11] = short_name(name)
        e[11] = 0x10
        e[20:22] = struct.pack("<H", c >> 16)
        e[26:28] = struct.pack("<H", c & 0xffff)
        p[slot:slot+32] = e
        write_cluster(parent, p)
        return c

    def put_file(parent, name, path):
        data = open(path, "rb").read()
        old = find_short(parent, short_name(name))
        if old:
            raise SystemExit(f"{name} already exists")
        count = max(1, (len(data) + cluster_bytes - 1) // cluster_bytes)
        chain = alloc_clusters(count)
        for i, c in enumerate(chain):
            chunk = data[i*cluster_bytes:(i+1)*cluster_bytes]
            write_cluster(c, chunk.ljust(cluster_bytes, b"\0"))
        p = read_cluster(parent)
        slot = None
        for off in range(0, cluster_bytes, 32):
            if p[off] in (0x00, 0xe5):
                slot = off
                break
        if slot is None:
            raise SystemExit("directory cluster full")
        e = bytearray(32)
        e[:11] = short_name(name)
        e[11] = 0x20
        e[20:22] = struct.pack("<H", chain[0] >> 16)
        e[26:28] = struct.pack("<H", chain[0] & 0xffff)
        e[28:32] = struct.pack("<I", len(data))
        p[slot:slot+32] = e
        write_cluster(parent, p)
        return chain

    # ROOT is the FAT32 root directory.
    boot = find_short(root, b"BOOT       ")
    if boot:
        boot_cluster = (boot[2][20] << 8 | boot[2][21]) << 16 | (boot[2][26] | boot[2][27] << 8)
    else:
        boot_cluster = make_dir(root, "BOOT")

    kernel_chain = put_file(boot_cluster, "KERNEL.ELF", kernel_path)

    # Build the NullL manifest from the actual FAT cluster chain.
    extents = []
    for c in kernel_chain:
        lba = data_lba + (c - 2) * spc
        count = spc
        if extents and extents[-1][0] + extents[-1][1] == lba:
            extents[-1] = (extents[-1][0], extents[-1][1] + count)
        else:
            extents.append((lba, count))

    if len(extents) > 30:
        raise SystemExit("kernel is too fragmented for NullL manifest")

    manifest = bytearray(SECTOR)
    size = os.path.getsize(kernel_path)
    struct.pack_into("<IIII", manifest, 0, 0x4b4c4c4e, 1, size, len(extents))
    for i, (lba, count) in enumerate(extents):
        struct.pack_into("<QII", manifest, 16 + i*16, lba, count, 0)

    f.seek((PART + 3) * SECTOR)
    f.write(manifest)
    print(f"[*] NullL kernel manifest: {len(extents)} extent(s), {size} bytes")
PY

echo "[*] saving Old Boot Record at partition-1"
dd if="$IMG" of="$IMG" bs=512 skip=0 seek=$((PARTITION_START - 1)) count=1 conv=notrunc status=none

echo "[*] installing NullL stage1"
dd if="$STAGE1" of="$IMG" bs=446 count=1 conv=notrunc status=none
echo "[*] installing NullL stage2"
dd if="$STAGE2" of="$IMG" bs=512 seek=$((PARTITION_START + STAGE2_OFFSET)) count=48 conv=notrunc status=none
dd if=/dev/zero of="$IMG" bs=512 seek=$((PARTITION_START + MAILBOX_OFFSET)) count=1 conv=notrunc status=none

python3 - "$IMG" "$KERNEL" <<'PY'
import os, struct, sys
img_path, kernel_path = sys.argv[1], sys.argv[2]
part = 2048 * 512
manifest_lba = 2048 + 3
with open(img_path, "r+b") as f:
    f.seek(part)
    bpb = f.read(512)
    bps = struct.unpack_from("<H", bpb, 11)[0]
    spc = bpb[13]
    reserved = struct.unpack_from("<H", bpb, 14)[0]
    fats = bpb[16]
    fat_size = struct.unpack_from("<I", bpb, 36)[0]
    root = struct.unpack_from("<I", bpb, 44)[0]
    if bps != 512 or spc == 0 or fats == 0 or fat_size == 0:
        raise SystemExit("invalid FAT32 BPB")
    fat_base = part + reserved * bps
    data_base_lba = 2048 + reserved + fats * fat_size
    def fat_next(cluster):
        f.seek(fat_base + cluster * 4)
        return struct.unpack("<I", f.read(4))[0] & 0x0fffffff
    def cluster_lba(cluster):
        return data_base_lba + (cluster - 2) * spc
    def read_cluster(cluster):
        f.seek(cluster_lba(cluster) * bps)
        return f.read(spc * bps)
    def find_entry(start, wanted):
        cluster, seen = start, set()
        while cluster >= 2 and cluster < 0x0ffffff8 and cluster not in seen:
            seen.add(cluster)
            data = read_cluster(cluster)
            for off in range(0, len(data), 32):
                e = data[off:off+32]
                if len(e) < 32 or e[0] in (0x00, 0xe5):
                    if e and e[0] == 0x00: break
                    continue
                if e[11] == 0x0f: continue
                if e[:11] == wanted: return e
            cluster = fat_next(cluster)
        return None
    boot = find_entry(root, b"BOOT       ")
    if boot is None: raise SystemExit("BOOT directory not found")
    boot_cluster = (struct.unpack_from("<H", boot, 20)[0] << 16) | struct.unpack_from("<H", boot, 26)[0]
    ker = find_entry(boot_cluster, b"KERNEL  ELF")
    if ker is None: raise SystemExit("BOOT/KERNEL.ELF not found")
    kernel_cluster = (struct.unpack_from("<H", ker, 20)[0] << 16) | struct.unpack_from("<H", ker, 26)[0]
    kernel_size = struct.unpack_from("<I", ker, 28)[0]
    if kernel_size != os.path.getsize(kernel_path):
        raise SystemExit("kernel size mismatch")
    remaining = (kernel_size + 511) // 512
    extents, seen, cluster = [], set(), kernel_cluster
    while remaining and cluster >= 2 and cluster < 0x0ffffff8 and cluster not in seen:
        seen.add(cluster)
        lba = cluster_lba(cluster)
        count = min(spc, remaining)
        if extents and extents[-1][0] + extents[-1][1] == lba:
            extents[-1] = (extents[-1][0], extents[-1][1] + count)
        else:
            extents.append((lba, count))
        remaining -= count
        cluster = fat_next(cluster)
    if remaining: raise SystemExit("kernel FAT chain ended early")
    if len(extents) > 30: raise SystemExit("kernel is too fragmented for NullL manifest")
    manifest = bytearray(512)
    struct.pack_into("<IIII", manifest, 0, 0x4b4c4c4e, 1, kernel_size, len(extents))
    off = 16
    for lba, count in extents:
        struct.pack_into("<QII", manifest, off, lba, count, 0)
        off += 16
    f.seek(manifest_lba * 512)
    f.write(manifest)
    print(f"[*] NullL kernel manifest: {len(extents)} extent(s), {kernel_size} bytes")
PY
echo "[*] NullL installation complete: $IMG"
