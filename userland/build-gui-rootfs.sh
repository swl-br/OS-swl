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

# Apps nativas i386 (compiladas no chroot pelo scripts/build-gui-i386.sh).
# Sem elas, clicar nos ícones do desktop cai em "tswl: not found".
if [ -d "$ARTIFACTS/apps" ]; then
    for app in "$ARTIFACTS"/apps/*; do
        [ -f "$app" ] || continue
        cp "$app" "$ROOTFS/bin/$(basename "$app")"
        chmod 755 "$ROOTFS/bin/$(basename "$app")"
    done
fi
# Catálogo: TSWL precisa estar em /bin (desktop aponta /bin/tswl).
if [ ! -x "$ROOTFS/bin/tswl" ]; then
    echo "AVISO: $ROOTFS/bin/tswl ausente — compile apps no chroot (build-gui-i386) e reempacote."
fi
if [ -x "$ROOTFS/bin/tswl" ]; then
    echo "  app tswl: $(ls -l "$ROOTFS/bin/tswl" | awk '{print $5, $NF}')"
fi

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

# Tema de cursor SWL: swlwm.c procura em /usr/share/swl-ui/cursors/
# (e /usr/local/share/swl-ui/cursors) por "$XCURSOR_PATH/swl/cursors/"
# Gerado por swl-ui/tools/gen-cursors/generate.sh — sem isso o wlroots
# cai no tema padrão (ou nenhum cursor de resize aparece).
if [ -d "$ASSETS_SRC/cursors/swl" ]; then
    mkdir -p "$ROOTFS/usr/share/swl-ui/cursors/swl/cursors"
    cp "$ASSETS_SRC/cursors/swl/index.theme" "$ROOTFS/usr/share/swl-ui/cursors/swl/"
    cp "$ASSETS_SRC"/cursors/swl/cursors/* "$ROOTFS/usr/share/swl-ui/cursors/swl/cursors/" 2>/dev/null || true
fi

# GUI-012: icons-pack — PNGs avulsos pra uso futuro (seletor de ícones,
# novos atalhos). `cp -n` de propósito: nunca sobrescreve os ícones reais
# dos atalhos do desktop instalados acima.
if [ -d "$ASSETS_SRC/icons-pack" ]; then
    mkdir -p "$ROOTFS/usr/share/swl-ui/icons-pack"
    cp -n "$ASSETS_SRC"/icons-pack/*.png "$ROOTFS/usr/share/swl-ui/icons-pack/" 2>/dev/null || true
fi

# xkb-data (arquivos de dados, independentes de arquitetura): sem
# /usr/share/X11/xkb o libxkbcommon não compila o keymap e o swlwm morre
# no boot (segfault com alguns builds). Prefere o do dir de artefatos
# (chroot i386) e cai pro do host como fallback.
if [ -d "$ARTIFACTS/xkb" ]; then
    mkdir -p "$ROOTFS/usr/share/X11"
    cp -a "$ARTIFACTS/xkb" "$ROOTFS/usr/share/X11/xkb"
elif [ -d /usr/share/X11/xkb ]; then
    mkdir -p "$ROOTFS/usr/share/X11"
    cp -a /usr/share/X11/xkb "$ROOTFS/usr/share/X11/xkb"
fi

# W2: quirks do libinput (independentes de arquitetura): sem
# /usr/share/libinput o backend loga "Failed to load the device quirks"
# e o comportamento de mouse/teclado degrada. Mesmo padrão do xkb acima.
if [ -d "$ARTIFACTS/libinput" ]; then
    mkdir -p "$ROOTFS/usr/share"
    cp -a "$ARTIFACTS/libinput" "$ROOTFS/usr/share/libinput"
elif [ -d /usr/share/libinput ]; then
    mkdir -p "$ROOTFS/usr/share"
    cp -a /usr/share/libinput "$ROOTFS/usr/share/libinput"
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
	<!-- T3: monospace e default com cobertura de block elements (Neo) -->
	<alias>
		<family>monospace</family>
		<prefer>
			<family>DejaVu Sans Mono</family>
			<family>DejaVu Sans</family>
			<family>JetBrains Mono</family>
		</prefer>
	</alias>
	<alias>
		<family>sans-serif</family>
		<prefer>
			<family>DejaVu Sans</family>
		</prefer>
	</alias>
</fontconfig>
EOF
mkdir -p "$ROOTFS/var/cache/fontconfig"

# T3: se o pacote de artefatos não trouxe Mono, tenta o host (Debian/Ubuntu)
if ! ls "$ROOTFS/usr/share/fonts"/DejaVuSansMono*.ttf >/dev/null 2>&1; then
    for hostf in \
        /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf \
        /usr/share/fonts/dejavu/DejaVuSansMono.ttf \
        /usr/share/fonts/TTF/DejaVuSansMono.ttf
    do
        if [ -f "$hostf" ]; then
            cp "$hostf" "$ROOTFS/usr/share/fonts/"
            echo "  fonte Mono do host: $hostf"
            break
        fi
    done
fi
if ! ls "$ROOTFS/usr/share/fonts"/DejaVuSans.ttf >/dev/null 2>&1 \
   && ! ls "$ROOTFS/usr/share/fonts"/DejaVuSans*.ttf >/dev/null 2>&1; then
    for hostf in \
        /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
        /usr/share/fonts/dejavu/DejaVuSans.ttf
    do
        if [ -f "$hostf" ]; then
            cp "$hostf" "$ROOTFS/usr/share/fonts/"
            echo "  fonte Sans do host: $hostf"
            break
        fi
    done
fi

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
# /dev/shm: o devtmpfs não cria; sem ele o wlroots falha em
# "allocate shm file for keymap" e o xkbcommon pode morrer depois.
mkdir -p /run/shm
ln -sfn /run/shm /dev/shm
export PATH="/bin:/usr/bin:/sbin${PATH:+:$PATH}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run}"
/sbin/systemd-udevd --daemon 2>/dev/null
/usr/bin/udevadm trigger --type=subsystems --action=add >/dev/null 2>&1
/usr/bin/udevadm trigger --type=devices --action=add >/dev/null 2>&1
/usr/bin/udevadm settle --timeout=5 >/dev/null 2>&1
exec /bin/swlwm || exec /bin/sh
EOF
chmod 755 "$ROOTFS/sbin/start-gui.sh"

n_libs=$(find "$ROOTFS/lib/i386-linux-gnu" -type f | wc -l)
echo "GUI empacotada em $ROOTFS: bin/swlwm + $n_libs libs + $(find "$ROOTFS/usr/share/fonts" -type f | wc -l) fonte(s) + udev"
