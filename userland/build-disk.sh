#!/bin/bash
set -e

BOOT_BIN="$1"
STAGE2_BIN="$2"
KERNEL="$3"
INITRD="$4"
OUT="$5"

if [ -z "$OUT" ]; then
    echo "uso: $0 <boot.bin> <stage2.bin> <bzImage> <initramfs.cpio.gz> <disco_saida.img>"
    exit 1
fi

SECTOR=512
KERNEL_LBA=10
KERNEL_BYTES=$(stat -c%s "$KERNEL")
KERNEL_SECTORS=$(( (KERNEL_BYTES + SECTOR - 1) / SECTOR ))
INITRD_LBA=$(( KERNEL_LBA + KERNEL_SECTORS ))
INITRD_BYTES=$(stat -c%s "$INITRD")
INITRD_SECTORS=$(( (INITRD_BYTES + SECTOR - 1) / SECTOR ))
TOTAL_SECTORS=$(( INITRD_LBA + INITRD_SECTORS + 64 ))

rm -f "$OUT"
dd if=/dev/zero of="$OUT" bs=512 count=$TOTAL_SECTORS status=none

dd if="$BOOT_BIN" of="$OUT" bs=512 seek=0 conv=notrunc status=none
dd if="$STAGE2_BIN" of="$OUT" bs=512 seek=1 conv=notrunc status=none

python3 - "$OUT" "$KERNEL_LBA" "$KERNEL_SECTORS" "$INITRD_LBA" "$INITRD_SECTORS" "$INITRD_BYTES" << 'PYEOF'
import struct, sys
out, klba, ksec, ilba, isec, ibytes = sys.argv[1:7]
manifest = struct.pack("<IIIII", int(klba), int(ksec), int(ilba), int(isec), int(ibytes))
manifest = manifest.ljust(512, b"\x00")
with open(out, "r+b") as f:
    f.seek(9 * 512)
    f.write(manifest)
PYEOF

dd if="$KERNEL" of="$OUT" bs=512 seek=$KERNEL_LBA conv=notrunc status=none
dd if="$INITRD" of="$OUT" bs=512 seek=$INITRD_LBA conv=notrunc status=none

echo "disco montado: $OUT"
echo "kernel: LBA $KERNEL_LBA, $KERNEL_SECTORS setores"
echo "initrd: LBA $INITRD_LBA, $INITRD_SECTORS setores"
