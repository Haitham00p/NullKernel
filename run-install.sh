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
python3 - "$IMG" "$KERNEL" "$SCRIPT_DIR" <<'PY'
import hashlib, os, re, struct, sys

img_path, kernel_path = sys.argv[1], sys.argv[2]
script_dir = sys.argv[3] if len(sys.argv) > 3 else "."
PART = 2048
SECTOR = 512
part_off = PART * SECTOR

with open(img_path, "r+b") as f:
    f.seek(part_off)
    bpb = f.read(SECTOR)

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
        raw = f.read(4)
        if len(raw) != 4:
            raise SystemExit("short FAT read")
        return struct.unpack("<I", raw)[0] & 0x0fffffff

    def fat_set(c, v):
        # FAT32 keeps the top four bits reserved.
        value = (v & 0x0fffffff) | 0xf0000000
        for n in range(fats):
            f.seek(fat_base + n * fat_size * bps + c * 4)
            f.write(struct.pack("<I", value))

    def cluster_off(c):
        return data_off + (c - 2) * cluster_bytes

    def read_cluster(c):
        f.seek(cluster_off(c))
        data = f.read(cluster_bytes)
        if len(data) != cluster_bytes:
            raise SystemExit("short FAT32 cluster read")
        return bytearray(data)

    def write_cluster(c, data):
        if len(data) != cluster_bytes:
            raise SystemExit("internal FAT32 cluster size error")
        f.seek(cluster_off(c))
        f.write(data)

    def short_name(name):
        base, dot, ext = name.upper().partition(".")
        if len(base) > 8 or len(ext) > 3:
            raise SystemExit(f"8.3 name required: {name}")
        return (base.ljust(8) + ext.ljust(3)).encode("ascii")

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

    def first_cluster(e):
        return (struct.unpack_from("<H", e, 20)[0] << 16) | struct.unpack_from("<H", e, 26)[0]

    _next_free = [2]

    def alloc_clusters(count):
        free = []
        c = _next_free[0]
        seen_wrap = 0
        while len(free) < count and seen_wrap < 2:
            if c >= cluster_count + 2:
                c = 2
                seen_wrap += 1
                continue
            if fat_get(c) == 0:
                free.append(c)
            c += 1
        if len(free) != count:
            raise SystemExit("not enough free FAT32 clusters")
        for i, c in enumerate(free):
            fat_set(c, 0x0fffffff if i + 1 == len(free) else free[i + 1])
        _next_free[0] = free[-1] + 1
        return free

    def free_chain(start):
        c, seen = start, set()
        while 2 <= c < 0x0ffffff8 and c not in seen:
            seen.add(c)
            nxt = fat_get(c)
            fat_set(c, 0)
            c = nxt
        _next_free[0] = min(_next_free[0], start)

    _LEGAL = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-$'~`!@#%^&(){}")

    def short_83(name):
        """Map a source-tree name onto a unique, valid FAT32 8.3 name.

        Names that already fit 8.3 are passed through unchanged. Longer ones
        are shortened and disambiguated with a hash of the full name, because
        plain truncation is lossy (KERNEL_DEBUGGING.TXT and
        KERNEL_DEVELOPMENT.TXT both collapse to KERNEL_D.TXT).
        """
        up = name.upper()
        base, dot, ext = up.rpartition(".")
        if not dot:
            base, ext = ext, ""
        base = "".join(ch if ch in _LEGAL else "_" for ch in base)
        ext = "".join(ch if ch in _LEGAL else "_" for ch in ext)
        if 1 <= len(base) <= 8 and len(ext) <= 3:
            return (base.ljust(8) + ext.ljust(3)).encode("ascii")
        digest = hashlib.sha1(name.encode("utf-8", "replace")).hexdigest()
        b = ((base[:2] or "_") + "_" + digest[:3])
        return (b.ljust(8) + ext[:3].ljust(3)).encode("ascii")

    def dir_slots(parent):
        """Walk a whole directory chain, yielding (cluster, offset, entry)."""
        c, seen = parent, set()
        while 2 <= c < 0x0ffffff8 and c not in seen:
            seen.add(c)
            data = read_cluster(c)
            for off in range(0, cluster_bytes, 32):
                yield c, off, data[off:off+32]
            c = fat_get(c)

    def dir_lookup(parent, sname):
        for c, off, e in dir_slots(parent):
            if e[0] == 0x00:
                return None
            if e[0] == 0xe5 or e[11] == 0x0f:
                continue
            if e[:11] == sname:
                return c, off, e
        return None

    def dir_ensure_slot(parent):
        """Find a reusable directory slot, growing the chain if needed."""
        for c, off, e in dir_slots(parent):
            if e[0] in (0x00, 0xe5):
                return c, off
        last, seen, c = parent, set(), parent
        while 2 <= c < 0x0ffffff8 and c not in seen:
            seen.add(c)
            last = c
            c = fat_get(c)
        fresh = alloc_clusters(1)[0]
        fat_set(last, fresh)
        write_cluster(fresh, bytes(cluster_bytes))
        return fresh, 0

    def dir_store(parent, sname, attr, first, size):
        hit = dir_lookup(parent, sname)
        if hit:
            c, off = hit[0], hit[1]
            entry = bytearray(hit[2])
        else:
            c, off = dir_ensure_slot(parent)
            entry = bytearray(32)
        entry[:11] = sname
        entry[11] = attr
        entry[20:22] = struct.pack("<H", first >> 16)
        entry[26:28] = struct.pack("<H", first & 0xffff)
        entry[28:32] = struct.pack("<I", size)
        data = bytearray(read_cluster(c))
        data[off:off+32] = entry
        write_cluster(c, data)

    def make_dir(parent, name):
        sname = short_83(name)
        hit = dir_lookup(parent, sname)
        if hit:
            return first_cluster(hit[2])

        c = alloc_clusters(1)[0]
        data = bytearray(cluster_bytes)
        data[0:11] = b".          "
        data[11] = 0x10
        data[20:22] = struct.pack("<H", c >> 16)
        data[26:28] = struct.pack("<H", c & 0xffff)
        data[32:43] = b"..         "
        data[43] = 0x10
        data[52:54] = struct.pack("<H", parent >> 16)
        data[58:60] = struct.pack("<H", parent & 0xffff)
        write_cluster(c, data)
        dir_store(parent, sname, 0x10, c, 0)
        return c

    def put_file(parent, name, path=None, data=None):
        if data is None:
            data = open(path, "rb").read()
        sname = short_83(name)
        hit = dir_lookup(parent, sname)
        if hit:
            # Re-installing over a used image: reclaim the old chain first so
            # a changed file is actually replaced instead of aborting.
            free_chain(first_cluster(hit[2]))
        count = max(1, (len(data) + cluster_bytes - 1) // cluster_bytes)
        chain = alloc_clusters(count)
        for i, c in enumerate(chain):
            chunk = data[i * cluster_bytes:(i + 1) * cluster_bytes]
            write_cluster(c, chunk.ljust(cluster_bytes, b"\x00"))
        dir_store(parent, sname, 0x20, chain[0], len(data))
        return chain

    def chain_from_fat(start):

        out, seen, c = [], set(), start
        while 2 <= c < 0x0ffffff8 and c not in seen:
            seen.add(c)
            out.append(c)
            c = fat_get(c)
        if c in seen:
            raise SystemExit("FAT32 cluster loop detected")
        if c < 0x0ffffff8:
            raise SystemExit(f"FAT32 chain points to invalid cluster {c}")
        return out

    # Stage the source rootfs/ tree so a fresh install is a usable filesystem
    # instead of just the kernel. sync_rootfs.sh is the reverse direction.
    src_root = os.path.join(script_dir, "rootfs")
    if not os.path.isdir(src_root):
        src_root = os.environ.get("NULLOS_ROOTFS", "")
    stats = {"dirs": 0, "files": 0, "bytes": 0}

    def stage_tree(parent, path):
        for entry in sorted(os.listdir(path)):
            full = os.path.join(path, entry)
            if os.path.islink(full) or not os.path.isfile(full):
                continue
            try:
                chain = put_file(parent, entry, path=full)
            except SystemExit as exc:
                print(f"[!] skipped /{entry}: {exc}")
                continue
            stats["files"] += 1
            stats["bytes"] += os.path.getsize(full)
            _ = chain
        for entry in sorted(os.listdir(path)):
            full = os.path.join(path, entry)
            if os.path.islink(full) or not os.path.isdir(full):
                continue
            if entry.upper() == "BOOT":
                # The loader directory holds the build artifact only.
                continue
            stage_tree(make_dir(parent, entry), full)
            stats["dirs"] += 1

    if src_root and os.path.isdir(src_root):
        print(f"[*] staging rootfs from {src_root}")
        stage_tree(root, src_root)
        print(f"[*] rootfs staged: {stats['dirs']} dirs, "
              f"{stats['files']} files, {stats['bytes']} bytes")
    else:
        print("[*] no rootfs/ source tree found; installing kernel only")

    boot = find_short(root, b"BOOT       ")
    boot_cluster = first_cluster(boot[2]) if boot else make_dir(root, "BOOT")
    kernel_chain = put_file(boot_cluster, "KERNEL.ELF", kernel_path)

    # Re-read the FAT chain from the directory entry. This catches corruption
    # before the manifest is emitted and guarantees the manifest matches FAT.
    boot_entry = find_short(root, b"BOOT       ")
    kernel_entry = find_short(first_cluster(boot_entry[2]), b"KERNEL  ELF")
    if kernel_entry is None:
        raise SystemExit("BOOT/KERNEL.ELF directory entry missing")

    actual = chain_from_fat(first_cluster(kernel_entry[2]))
    if len(actual) != len(kernel_chain):
        raise SystemExit(
            f"kernel FAT chain length mismatch: wrote {len(kernel_chain)}, read {len(actual)}"
        )

    extents = []
    for c in actual:
        lba = data_lba + (c - 2) * spc
        count = spc
        if extents and extents[-1][0] + extents[-1][1] == lba:
            extents[-1] = (extents[-1][0], extents[-1][1] + count)
        else:
            extents.append((lba, count))

    if len(extents) > 30:
        raise SystemExit("kernel is too fragmented for NullL manifest")

    size = os.path.getsize(kernel_path)
    manifest = bytearray(SECTOR)
    struct.pack_into("<IIII", manifest, 0, 0x4b4c4c4e, 1, size, len(extents))
    for i, (lba, count) in enumerate(extents):
        struct.pack_into("<QII", manifest, 16 + i * 16, lba, count, 0)

    f.seek((PART + 3) * SECTOR)
    f.write(manifest)
    f.flush()

    print(f"[*] NullL kernel manifest: {len(extents)} extent(s), {size} bytes")
PY

echo "[*] saving Old Boot Record at partition-1"
dd if="$IMG" of="$IMG" bs=512 skip=0 seek=$((PARTITION_START - 1)) count=1 conv=notrunc status=none

echo "[*] installing NullL stage1"
dd if="$STAGE1" of="$IMG" bs=446 count=1 conv=notrunc status=none
echo "[*] installing NullL stage2"
dd if="$STAGE2" of="$IMG" bs=512 seek=$((PARTITION_START + STAGE2_OFFSET)) count=48 conv=notrunc status=none
dd if=/dev/zero of="$IMG" bs=512 seek=$((PARTITION_START + MAILBOX_OFFSET)) count=1 conv=notrunc status=none

echo "[*] NullL installation complete: $IMG"
