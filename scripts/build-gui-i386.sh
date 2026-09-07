#!/bin/bash
# build-gui-i386.sh — GUI-003
#
# Automatiza, do zero, o roteiro validado manualmente na sessão de
# investigação da GUI-003 (ver docs/ai/work/GUI-003-FIRST-GRAPHICAL-BOOT.md):
#
#   1. monta um chroot Debian trixie i386 (native build, não cross-compile)
#   2. compila uma libwlroots-0.18 MÍNIMA a partir do source oficial
#      (sem X11, sem GLES2/Vulkan/GBM — só DRM+libinput+Pixman)
#   3. compila o swl-ui/swlwm contra essa lib mínima
#   4. copia o binário + todas as .so realmente carregadas (via ldd,
#      dentro do chroot) pra um diretório de artefatos, prontos pra
#      o userland/build-gui-rootfs.sh empacotar no rootfs do SWL OS
#
# Precisa rodar como root, numa máquina com internet de verdade
# (Xubuntu/Mint — não roda dentro de ambientes com rede restrita).
#
# Uso:
#   sudo ./scripts/build-gui-i386.sh [diretorio_chroot] [diretorio_saida]
#
# Padrão: chroot em /srv/chroot-trixie-i386, artefatos em ./gui-artifacts

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CHROOT="${1:-/srv/chroot-trixie-i386}"
OUT="${2:-$REPO_ROOT/gui-artifacts}"

if [ "$(id -u)" -ne 0 ]; then
    echo "precisa rodar como root (sudo $0 ...)" >&2
    exit 1
fi

log() { echo ">>> $*"; }

# --- 1. chroot ---------------------------------------------------------

if [ ! -d "$CHROOT/usr" ]; then
    log "criando chroot Debian trixie i386 em $CHROOT (debootstrap)..."
    apt-get install -y debootstrap >/dev/null
    debootstrap --arch=i386 trixie "$CHROOT" http://deb.debian.org/debian
else
    log "chroot já existe em $CHROOT, reaproveitando"
fi

cp /etc/resolv.conf "$CHROOT/etc/resolv.conf"

for d in proc sys dev; do
    mountpoint -q "$CHROOT/$d" || mount --bind "/$d" "$CHROOT/$d"
done

# deb-src é exigido pelo apt-get source do wlroots, e não vem
# habilitado por padrão no debootstrap
if ! grep -q "^deb-src" "$CHROOT/etc/apt/sources.list" 2>/dev/null; then
    echo "deb-src http://deb.debian.org/debian trixie main" >> "$CHROOT/etc/apt/sources.list"
fi

# --- 2. código-fonte do swl-ui dentro do chroot -------------------------

rm -rf "$CHROOT/root/swl-ui"
cp -r "$REPO_ROOT/swl-ui" "$CHROOT/root/swl-ui"

# Apps nativas (tswl, swlpad): mesmo tratamento — copiadas pra dentro pra
# compilar native i386 junto (os binários em apps/*/build/ são do host
# x86_64 e NÃO rodam no guest; nunca copiá-los pro rootfs).
rm -rf "$CHROOT/root/apps"
cp -r "$REPO_ROOT/apps" "$CHROOT/root/apps"

# --- 3. tudo que roda DENTRO do chroot (native build i386) --------------
# Isolado num heredoc pra rodar como um script só, sem depender de
# múltiplas invocações de `chroot` (evita o erro comum de "rodei fora
# do chroot sem perceber").

cat > "$CHROOT/root/inside-chroot-build.sh" << 'INSIDE'
#!/bin/bash
set -euo pipefail

echo ">>> [dentro do chroot] confirmando ambiente:"
cat /etc/debian_version
uname -m 2>/dev/null || true

export DEBIAN_FRONTEND=noninteractive
apt-get update

apt-get install -y --no-install-recommends \
    build-essential meson ninja-build pkg-config file hwdata \
    libwlroots-0.18-dev libwayland-dev wayland-protocols libxkbcommon-dev \
    libcairo2-dev libpango1.0-dev libdrm-dev fontconfig fonts-dejavu-core

# --- wlroots mínimo, a partir do source oficial do Debian ---
cd /root
rm -rf wlroots-*/
apt-get source wlroots
cd wlroots-*/

meson setup build-minimal \
    -Drenderers=[] \
    -Dbackends=drm,libinput \
    -Dallocators=[] \
    -Dsession=enabled \
    -Dxwayland=disabled \
    -Dxcb-errors=disabled \
    -Dexamples=false \
    --prefix=/usr/local

ninja -C build-minimal
ninja -C build-minimal install
ldconfig

# --- swl-ui contra o wlroots mínimo ---
cd /root/swl-ui
rm -rf build
PKG_CONFIG_PATH=/usr/local/lib/i386-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig \
    meson setup build
ninja -C build

# --- apps nativas (tswl, swlpad) — mesmos clientes Wayland do desktop ---
for app in tswl swlpad; do
    cd /root/apps/$app
    rm -rf build
    meson setup build
    ninja -C build
    file build/$app
done
cd /root/swl-ui

apt-get install -y --no-install-recommends udev

echo ">>> [dentro do chroot] resultado:"
file build/swlwm
ldd build/swlwm | wc -l

# --- coleta os artefatos: binário + todas as .so carregadas ---
mkdir -p /root/gui-artifacts/lib /root/gui-artifacts/apps
cp build/swlwm /root/gui-artifacts/swlwm
for app in tswl swlpad; do
    cp /root/apps/$app/build/$app /root/gui-artifacts/apps/$app
done

# A4.1: udev — sem isso, /dev/input/event* existe (via devtmpfs) mas o
# libinput não sabe que são teclado/mouse (essa classificação vem das
# regras de udev + o builtin input_id, gravada no banco de dados do
# udev — não é algo que o devtmpfs sozinho faz). No Debian/Ubuntu, o
# daemon e o utilitário de linha de comando são o MESMO binário ELF —
# ele decide se age como "systemd-udevd" (daemon) ou "udevadm" (CLI)
# olhando o nome pelo qual foi invocado (argv[0]), não o caminho. Por
# isso copiamos o mesmo arquivo duas vezes, com nomes diferentes, em
# vez de link simbólico (mais robusto num rootfs minimalista).
UDEVADM_BIN="$(readlink -f /usr/bin/udevadm)"
mkdir -p /root/gui-artifacts/udev/rules.d
cp "$UDEVADM_BIN" /root/gui-artifacts/udev/udevadm
cp "$UDEVADM_BIN" /root/gui-artifacts/udev/systemd-udevd
cp /usr/lib/udev/rules.d/60-input-id.rules /root/gui-artifacts/udev/rules.d/ 2>/dev/null || true
cp /usr/lib/udev/rules.d/60-persistent-input.rules /root/gui-artifacts/udev/rules.d/ 2>/dev/null || true

ldd build/swlwm | awk '/=>/ {print $3} !/=>/ && /\// {print $1}' \
    | grep -v '^$' | sort -u | while read -r lib; do
        [ -f "$lib" ] && cp -L "$lib" /root/gui-artifacts/lib/
done
for app in tswl swlpad; do
    ldd /root/apps/$app/build/$app | awk '/=>/ {print $3} !/=>/ && /\// {print $1}' \
        | grep -v '^$' | sort -u | while read -r lib; do
            [ -f "$lib" ] && cp -L "$lib" /root/gui-artifacts/lib/
    done
done
ldd "$UDEVADM_BIN" | awk '/=>/ {print $3} !/=>/ && /\// {print $1}' \
    | grep -v '^$' | sort -u | while read -r lib; do
        [ -f "$lib" ] && cp -L "$lib" /root/gui-artifacts/lib/
    done

# linker dinâmico em si (o ELF referencia /lib/ld-linux.so.2 direto,
# não aparece na lista de "=>" do ldd)
cp -L /lib/ld-linux.so.2 /root/gui-artifacts/lib/ 2>/dev/null || \
    cp -L /lib/i386-linux-gnu/ld-linux.so.2 /root/gui-artifacts/lib/

# fonte mínima — o painel/taskbar desenham texto real via Pango, sem
# isso o fontconfig não acha nada pra renderizar
mkdir -p /root/gui-artifacts/fonts
find /usr/share/fonts -iname "DejaVuSans.ttf" -exec cp {} /root/gui-artifacts/fonts/ \;

echo ">>> [dentro do chroot] artefatos em /root/gui-artifacts:"
du -sh /root/gui-artifacts
ls /root/gui-artifacts/lib | wc -l
INSIDE

chmod +x "$CHROOT/root/inside-chroot-build.sh"

log "rodando o build dentro do chroot (native i386, pode levar alguns minutos)..."
chroot "$CHROOT" /root/inside-chroot-build.sh

# --- 4. trazer os artefatos pra fora do chroot ---------------------------

rm -rf "$OUT"
mkdir -p "$OUT"
cp -r "$CHROOT/root/gui-artifacts/." "$OUT/"

log "artefatos prontos em: $OUT"
log "  $OUT/swlwm             — binário i386"
log "  $OUT/apps/tswl,swlpad  — apps nativas i386"
log "  $OUT/lib/*.so*         — todas as libs carregadas (i386)"
log "  $OUT/fonts/*.ttf       — fonte mínima pro Pango"
log "  $OUT/udev/             — udevadm/systemd-udevd + regras de input (A4.1)"
log ""
log "próximo passo: userland/build-gui-rootfs.sh <rootfs> $OUT"
