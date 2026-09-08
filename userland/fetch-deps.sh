#!/bin/sh
# Obtem as dependências externas que NÃO ficam commitadas no git.
# Uso:    ./userland/fetch-deps.sh
# Depois: make            (para buildar boot/stage2/init/disk)
#
# O que este script faz:
#   1. kernel-7.2.1 (fonte linux oficial) em kernel/linux-7.2.1
#   2. bash e busybox estáticos **i386** (para o rootfs do target)
#
# B3: o target do SWL OS é x86 32-bit. Compilar bash/busybox sem -m32
# num host x86_64 gera binários inexequíveis no kernel i386 (ENOEXEC,
# boot calado). Este script força ELF 32-bit e verifica o resultado.

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

# --- toolchain i386 -------------------------------------------------------
# Host pode ser x86_64; target do rootfs é sempre i386.
HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
    i386|i686)
        I386_CFLAGS="-Os -static"
        I386_LDFLAGS="-static"
        NEED_M32=0
        ;;
    *)
        I386_CFLAGS="-m32 -Os -static"
        I386_LDFLAGS="-m32 -static"
        NEED_M32=1
        ;;
esac

require_i386_cc() {
    if [ "$NEED_M32" -eq 0 ]; then
        return 0
    fi
    tmp_c="/tmp/swl-i386-check-$$.c"
    tmp_o="/tmp/swl-i386-check-$$"
    echo 'int main(void){return 0;}' > "$tmp_c"
    if ! cc $I386_CFLAGS $I386_LDFLAGS -o "$tmp_o" "$tmp_c" 2>/dev/null; then
        rm -f "$tmp_c" "$tmp_o"
        echo "ERRO: não consigo compilar binário i386 estático (cc -m32 -static)." >&2
        echo "      No Debian/Ubuntu/Mint instale:" >&2
        echo "        sudo apt-get install gcc-multilib libc6-dev-i386" >&2
        echo "      O SWL OS é 32-bit; bash/busybox do host x86_64 quebram o boot." >&2
        exit 1
    fi
    rm -f "$tmp_c" "$tmp_o"
}

assert_elf32() {
    path="$1"
    label="$2"
    if [ ! -f "$path" ]; then
        echo "ERRO: $label não existe em $path" >&2
        exit 1
    fi
    info=""
    if command -v readelf >/dev/null 2>&1; then
        # LC_ALL=C: readelf pt_BR imprime "Classe:" em vez de "Class:"
        # (B3 ressalva — rebuild redundante em locale não-C).
        info="$(LC_ALL=C readelf -h "$path" 2>/dev/null || true)"
        if echo "$info" | grep -qE 'Class(e)?:[[:space:]]*ELF32'; then
            echo "    $label: ELF32 ok ($(basename "$path"))"
            return 0
        fi
    fi
    if command -v file >/dev/null 2>&1; then
        info="$(file -b "$path" 2>/dev/null || true)"
        case "$info" in
            *"ELF 32-bit"*)
                echo "    $label: ELF32 ok ($(basename "$path"))"
                return 0
                ;;
        esac
    fi
    echo "ERRO: $label em $path NÃO é ELF 32-bit." >&2
    echo "      info: ${info:-desconhecida}" >&2
    echo "      Remova $path e rode de novo após instalar gcc-multilib." >&2
    exit 1
}

ensure_elf32_or_rebuild() {
    path="$1"
    label="$2"
    if [ ! -x "$path" ]; then
        return 1
    fi
    if command -v readelf >/dev/null 2>&1; then
        if LC_ALL=C readelf -h "$path" 2>/dev/null | grep -qE 'Class(e)?:[[:space:]]*ELF32'; then
            echo "    $label já presente e é ELF32 — reutilizando"
            return 0
        fi
    elif command -v file >/dev/null 2>&1; then
        case "$(file -b "$path" 2>/dev/null || true)" in
            *"ELF 32-bit"*)
                echo "    $label já presente e é ELF32 — reutilizando"
                return 0
                ;;
        esac
    else
        echo "    $label existe mas sem file/readelf pra validar — reconstruindo"
        rm -f "$path"
        return 1
    fi
    echo "    $label existente NÃO é ELF32 (provavelmente do host) — removendo"
    rm -f "$path"
    return 1
}

echo "==> toolchain i386"
require_i386_cc
echo "    ok (CFLAGS='$I386_CFLAGS')"

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

echo "==> bash estático i386"
if ! ensure_elf32_or_rebuild "$TOOLS/bash" "bash"; then
    cd "$TOOLS"
    if [ ! -d "bash-${BASH_VER}" ]; then
        wget -O bash.tar.gz "$BASH_TARBALL_URL"
        tar -xzf bash.tar.gz
    fi
    cd "bash-${BASH_VER}"
    ./configure \
        --host=i386-pc-linux-gnu \
        --enable-static-link \
        --without-bash-malloc \
        CC="cc" \
        CFLAGS="$I386_CFLAGS" \
        LDFLAGS="$I386_LDFLAGS"
    make -j"$(nproc)"
    cp bash "$TOOLS/bash"
    assert_elf32 "$TOOLS/bash" "bash"
fi

echo "==> busybox estático i386"
if ! ensure_elf32_or_rebuild "$TOOLS/busybox" "busybox"; then
    cd "$TOOLS"
    if [ ! -d "busybox-${BUSYBOX_VER}" ]; then
        wget -O busybox.tar.bz2 "$BUSYBOX_TARBALL_URL"
        tar -xjf busybox.tar.bz2
    fi
    cd "busybox-${BUSYBOX_VER}"
    make defconfig
    if grep -q '^CONFIG_STATIC=' .config 2>/dev/null; then
        sed -i 's/^CONFIG_STATIC=.*/CONFIG_STATIC=y/' .config
    else
        echo 'CONFIG_STATIC=y' >> .config
    fi
    sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config 2>/dev/null || true
    make -j"$(nproc)" \
        EXTRA_CFLAGS="$I386_CFLAGS" \
        EXTRA_LDFLAGS="$I386_LDFLAGS" \
        CFLAGS="$I386_CFLAGS" \
        LDFLAGS="$I386_LDFLAGS"
    cp busybox "$TOOLS/busybox"
    assert_elf32 "$TOOLS/busybox" "busybox"
fi

echo
echo "==> montando rootfs em rootfs/"
if [ ! -f "$ROOT/build/init" ]; then
    echo "ERRO: $ROOT/build/init não existe. Rode 'make' primeiro (gera boot/stage2/init)."
    exit 1
fi
assert_elf32 "$TOOLS/bash" "bash"
assert_elf32 "$TOOLS/busybox" "busybox"

"$ROOT/userland/build-rootfs.sh" \
    "$ROOT/rootfs" \
    "$TOOLS/bash" \
    "$TOOLS/busybox" \
    "$ROOT/build/init"

echo
echo "Feito. Agora rode:"
echo "  make -C $KERNEL_DIR -j\$(nproc)          # kernel -> bzImage"
echo "  make                                       # boot/stage2/init rootfs-assets + disk.img"
echo
echo "Lembrete: /bin/sh no rootfs aponta pro busybox i386 (não pro bash)."
echo "          Ver userland/build-rootfs.sh e sessão 2026-09-07."
