#!/bin/sh
# Obtem as dependências externas que NÃO ficam commitadas no git.
# Uso:    ./userland/fetch-deps.sh
# Depois: make            (para buildar boot/stage2/init/disk)
#
# O que este script faz:
#   1. kernel-7.2.1 (fonte linux oficial) em kernel/linux-7.2.1
#   2. bash e busybox estáticos (para o rootfs)
#
# NOTA: os binários de bash/busybox podem vir de um pacote estático da
# sua distro, ou dos tarballs oficiais abaixo (configurado para a versão
# usada no projeto).

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL_DIR="$ROOT/kernel/linux-7.2.1"
KERNEL_VER="7.2.1"
KERNEL_TARBALL_URL="https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-${KERNEL_VER}.tar.xz"
BASH_VER="5.3"
BASH_TARBALL_URL="https://ftp.gnu.org/gnu/bash/bash-${BASH_VER}.tar.gz"
BUSYBOX_VER="1.37.0"
BUSYBOX_TARBALL_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2"
TOOLS="$ROOT/build/tools"

mkdir -p "$TOOLS"

echo "==> Kernel $KERNEL_VER"
if [ ! -d "$KERNEL_DIR" ]; then
    mkdir -p "$(dirname "$KERNEL_DIR")"
    echo "    baixando $KERNEL_TARBALL_URL"
    wget -O /tmp/linux.tar.xz "$KERNEL_TARBALL_URL"
    tar -C "$(dirname "$KERNEL_DIR")" -xJf /tmp/linux.tar.xz
    rm -f /tmp/linux.tar.xz
    echo "    aplicando config do projeto (kernel/config/swl_defconfig)"
    make -C "$KERNEL_DIR" defconfig
    cp "$ROOT/kernel/config/swl_defconfig" "$KERNEL_DIR/.config"
fi
echo "    ok (use 'make -C $KERNEL_DIR -j\$(nproc)' uma vez para gerar arch/x86/boot/bzImage)"

echo "==> bash estático"
if [ ! -x "$TOOLS/bash" ]; then
    cd "$TOOLS"
    wget -O bash.tar.gz "$BASH_TARBALL_URL"
    tar -xzf bash.tar.gz
    cd "bash-${BASH_VER}"
    ./configure --enable-static-link --without-bash-malloc CFLAGS="-Os -static"
    make -j"$(nproc)"
    cp bash "$TOOLS/bash"
fi

echo "==> busybox estático"
if [ ! -x "$TOOLS/busybox" ]; then
    cd "$TOOLS"
    wget -O busybox.tar.bz2 "$BUSYBOX_TARBALL_URL"
    tar -xjf busybox.tar.bz2
    cd "busybox-${BUSYBOX_VER}"
    make defconfig
    sed -i 's/^CONFIG_STATIC.*/CONFIG_STATIC=y/' .config
    make -j"$(nproc)"
    cp busybox "$TOOLS/busybox"
fi

echo
echo "==> montando rootfs em rootfs/"
if [ ! -f "$ROOT/build/init" ]; then
    echo "ERRO: $ROOT/build/init não existe. Rode 'make' primeiro (gera boot/stage2/init)."
    exit 1
fi
"$ROOT/userland/build-rootfs.sh" \
    "$ROOT/rootfs" \
    "$TOOLS/bash" \
    "$TOOLS/busybox" \
    "$ROOT/build/init"

echo
echo "Feito. Agora rode:"
echo "  make -C $KERNEL_DIR -j\$(nproc)          # kernel -> bzImage"
echo "  make                                       # boot/stage2/init rootfs-assets + disk.img"