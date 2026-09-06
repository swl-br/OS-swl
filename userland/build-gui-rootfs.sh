#!/bin/sh
# build-gui-rootfs.sh — GUI-003
#
# Copia os artefatos gerados por scripts/build-gui-i386.sh (binário
# swlwm i386 + bibliotecas dinâmicas + fonte) pro rootfs do SWL OS,
# nos caminhos que o /lib/ld-linux.so.2 espera encontrar em runtime.
#
# Roda DEPOIS de userland/build-rootfs.sh (que cria a estrutura base
# do rootfs — /bin, /sbin, /etc, etc.).
#
# Uso:
#   ./userland/build-gui-rootfs.sh <destino_rootfs> <dir_gui-artifacts>

set -e

ROOTFS="$1"
ARTIFACTS="$2"

if [ -z "$ROOTFS" ] || [ -z "$ARTIFACTS" ]; then
    echo "uso: $0 <destino_rootfs> <dir_gui-artifacts>"
    echo "(dir_gui-artifacts é a saída do scripts/build-gui-i386.sh)"
    exit 1
fi

if [ ! -f "$ARTIFACTS/swlwm" ]; then
    echo "erro: $ARTIFACTS/swlwm não existe — rode scripts/build-gui-i386.sh primeiro" >&2
    exit 1
fi

# Mesma convenção do rootfs/ já existente (bin/bash, bin/busybox):
# binários em /bin, libs em /lib/i386-linux-gnu (multiarch, como o
# Debian empacota — mantemos o mesmo layout que o linker já espera
# em vez de inventar um esquema próprio).
mkdir -p "$ROOTFS/bin" "$ROOTFS/lib/i386-linux-gnu" \
         "$ROOTFS/usr/share/fonts" "$ROOTFS/run"

cp "$ARTIFACTS/swlwm" "$ROOTFS/bin/swlwm"
chmod 755 "$ROOTFS/bin/swlwm"

for lib in "$ARTIFACTS"/lib/*.so*; do
    [ -e "$lib" ] || continue
    base="$(basename "$lib")"
    if [ "$base" = "ld-linux.so.2" ]; then
        # o linker dinâmico em si vai em /lib, não em /lib/i386-linux-gnu
        # (é o caminho fixo (interpreter) gravado no próprio ELF)
        cp "$lib" "$ROOTFS/lib/ld-linux.so.2"
    else
        cp "$lib" "$ROOTFS/lib/i386-linux-gnu/$base"
    fi
done

for font in "$ARTIFACTS"/fonts/*.ttf; do
    [ -e "$font" ] || continue
    cp "$font" "$ROOTFS/usr/share/fonts/"
done

# Ícones/wallpaper: o swl-ui (desktop.c/swlwm.c) procura em
# /usr/share/swl-ui/ como um dos caminhos "instalados" — sem isso ele
# cai no glifo vetorial genérico em vez do ícone/wallpaper reais.
# Repo-relativo à raiz do projeto: swl-ui/assets/.
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS_SRC="$REPO_ROOT/swl-ui/assets"
if [ -d "$ASSETS_SRC" ]; then
    mkdir -p "$ROOTFS/usr/share/swl-ui/icons"
    [ -f "$ASSETS_SRC/wallpaper.png" ] && cp "$ASSETS_SRC/wallpaper.png" "$ROOTFS/usr/share/swl-ui/"
    if [ -d "$ASSETS_SRC/icons" ]; then
        cp "$ASSETS_SRC"/icons/*.png "$ROOTFS/usr/share/swl-ui/icons/" 2>/dev/null || true
    fi
fi

# fontconfig precisa de um config mínimo apontando pra essa pasta —
# sem isso ele não acha a fonte mesmo com o arquivo presente.
mkdir -p "$ROOTFS/etc/fonts"
cat > "$ROOTFS/etc/fonts/fonts.conf" << 'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
	<dir>/usr/share/fonts</dir>
	<cachedir>/var/cache/fontconfig</cachedir>
</fontconfig>
EOF
mkdir -p "$ROOTFS/var/cache/fontconfig"

# A4.1: udev — sem isso /dev/input/event* existe mas o libinput não
# reconhece nenhum como teclado/mouse. O mesmo binário serve como
# "systemd-udevd" (daemon) ou "udevadm" (CLI) dependendo do nome pelo
# qual é chamado — por isso são duas cópias do mesmo arquivo, não um
# link simbólico (mais robusto num rootfs minimalista).
if [ -f "$ARTIFACTS/udev/udevadm" ]; then
    mkdir -p "$ROOTFS/sbin" "$ROOTFS/usr/bin" "$ROOTFS/usr/lib/udev/rules.d" "$ROOTFS/run/udev"
    cp "$ARTIFACTS/udev/udevadm" "$ROOTFS/usr/bin/udevadm"
    cp "$ARTIFACTS/udev/systemd-udevd" "$ROOTFS/sbin/systemd-udevd"
    chmod 755 "$ROOTFS/usr/bin/udevadm" "$ROOTFS/sbin/systemd-udevd"
    for rule in "$ARTIFACTS"/udev/rules.d/*.rules; do
        [ -e "$rule" ] || continue
        cp "$rule" "$ROOTFS/usr/lib/udev/rules.d/"
    done
fi

# init.asm executa este script em vez do swlwm direto: primeiro sobe o
# udev e classifica os dispositivos de input (sem isso não há
# teclado/mouse — ver sessão docs/ai/sessions/2026-09-05-*), só então
# lança a GUI. Se qualquer coisa aqui falhar, cai pro shell — nunca
# trava o boot.
mkdir -p "$ROOTFS/sbin"
cat > "$ROOTFS/sbin/start-gui.sh" << 'EOF'
#!/bin/sh
mkdir -p /run/udev
/sbin/systemd-udevd --daemon 2>/dev/null
/usr/bin/udevadm trigger --type=subsystems --action=add >/dev/null 2>&1
/usr/bin/udevadm trigger --type=devices --action=add >/dev/null 2>&1
/usr/bin/udevadm settle --timeout=5 >/dev/null 2>&1
exec /bin/swlwm || exec /bin/sh
EOF
chmod 755 "$ROOTFS/sbin/start-gui.sh"

n_libs=$(find "$ROOTFS/lib/i386-linux-gnu" -type f | wc -l)
echo "GUI empacotada em $ROOTFS: bin/swlwm + $n_libs libs + $(find "$ROOTFS/usr/share/fonts" -type f | wc -l) fonte(s) + udev"
