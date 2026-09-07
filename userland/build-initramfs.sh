#!/bin/sh
# build-initramfs.sh — R-16
#
# Empacota um diretório de rootfs (montado por userland/build-rootfs.sh
# e depois userland/build-gui-rootfs.sh) num initramfs cpio.gz que o
# bootloader (stage2.asm) consegue carregar junto do kernel.
#
# Era a lacuna R-16: até 2026-09-06 não havia receita no repo — o
# build/initramfs.cpio.gz era feito à mão fora do git, então um clone
# limpo não reproduzia o boot. Agora `make build/initramfs.cpio.gz`
# (ou `make disk.img`) gera o initramfs a partir do rootfs.
#
# Uso:
#   ./userland/build-initramfs.sh <dir_rootfs> <saida.cpio.gz>

set -e

ROOTFS="$1"
OUT="$2"

if [ -z "$ROOTFS" ] || [ -z "$OUT" ]; then
    echo "uso: $0 <dir_rootfs> <saida.cpio.gz>" >&2
    echo "(rootfs é montado por userland/build-rootfs.sh + build-gui-rootfs.sh)" >&2
    exit 1
fi

if [ ! -d "$ROOTFS" ]; then
    echo "erro: $ROOTFS não existe — rode userland/build-rootfs.sh primeiro" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT")"
( cd "$ROOTFS" && find . -print0 | cpio --null -o -H newc 2>/dev/null ) | gzip -9 > "$OUT"
echo "initramfs gerado: $OUT ($(stat -c%s "$OUT") bytes)"