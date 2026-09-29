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

KERNEL="$SCRIPT_DIR/build/kernel.elf"
STAGE1="$SCRIPT_DIR/build/nulll/stage1.bin"
STAGE2="$SCRIPT_DIR/build/nulll/stage2.bin"
[ -f "$KERNEL" ] || { echo "error: $KERNEL missing; run make" >&2; exit 1; }
[ -f "$STAGE1" ] || { echo "error: $STAGE1 missing; run make nulll" >&2; exit 1; }
[ -f "$STAGE2" ] || { echo "error: $STAGE2 missing; run make nulll" >&2; exit 1; }

IMG_OFFSET="$IMG@@$((PARTITION_START * 512))"
echo "[*] staging NullOS FAT32 filesystem"
mmd -i "$IMG_OFFSET" ::/BOOT 2>/dev/null || true
mcopy -o -i "$IMG_OFFSET" "$KERNEL" ::/BOOT/KERNEL.ELF
if [ -d "$SCRIPT_DIR/rootfs" ]; then
    mcopy -s -o -i "$IMG_OFFSET" "$SCRIPT_DIR/rootfs"/* ::/ 2>/dev/null || true
    mmd -i "$IMG_OFFSET" ::/BOOT 2>/dev/null || true
    mcopy -o -i "$IMG_OFFSET" "$KERNEL" ::/BOOT/KERNEL.ELF
fi

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
