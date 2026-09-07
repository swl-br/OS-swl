#!/bin/sh
# generate.sh — (re)gera o tema de cursor "swl" a partir de gen_cursors.c.
#
# Requer: gcc, pkg-config, cairo (dev), xcursorgen (pacote x11-apps no
# Ubuntu/Debian: `apt install x11-apps`).
#
# Roda a partir de qualquer diretório — resolve os caminhos sozinho.
# Saída final: swl-ui/assets/cursors/swl/{cursors/*,index.theme}
#
# IMPORTANTE: os hotspots aqui embaixo (HOTSPOTS) precisam bater com os
# valores em gen_cursors.c (array CURSORS) — são a mesma informação
# duplicada em dois lugares porque um é C (desenho) e o outro é shell
# (empacotamento). Se mudar um hotspot num lugar, muda no outro.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SWLUI_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_THEME_DIR="$SWLUI_DIR/assets/cursors/swl"
TMP_DIR="$(mktemp -d)"

trap 'rm -rf "$TMP_DIR"' EXIT

echo "== Compilando gen_cursors =="
gcc -O2 -o "$TMP_DIR/gen_cursors" "$SCRIPT_DIR/gen_cursors.c" \
	$(pkg-config --cflags --libs cairo) -lm

echo "== Gerando PNGs em $TMP_DIR =="
"$TMP_DIR/gen_cursors" "$TMP_DIR"

mkdir -p "$OUT_THEME_DIR/cursors"

# nome-do-cursor:hotspot_x_em_24:hotspot_y_em_24 (mesmos valores de
# gen_cursors.c, ver aviso no topo do arquivo)
CURSORS="
default:2:2
text:12:12
pointer:9:2
grab:12:12
grabbing:12:12
wait:12:12
not-allowed:12:12
crosshair:12:12
ns-resize:12:12
ew-resize:12:12
nesw-resize:12:12
nwse-resize:12:12
"

echo "== Empacotando com xcursorgen =="
for entry in $CURSORS; do
	name="${entry%%:*}"
	rest="${entry#*:}"
	hx="${rest%%:*}"
	hy="${rest#*:}"

	cfg="$TMP_DIR/$name.cursorgen"
	: > "$cfg"
	for size in 24 48; do
		# hotspot escalado proporcionalmente ao tamanho real do PNG
		shx=$((hx * size / 24))
		shy=$((hy * size / 24))
		echo "$size $shx $shy $TMP_DIR/$name-$size.png" >> "$cfg"
	done

	xcursorgen "$cfg" "$OUT_THEME_DIR/cursors/$name"
	echo "  gerado: cursors/$name"
done

echo "== Criando aliases (symlinks) =="
cd "$OUT_THEME_DIR/cursors"
link() { ln -sf "$1" "$2"; echo "  $2 -> $1"; }

link default left_ptr
link default top_left_arrow
link default arrow

link text xterm
link text ibeam

link pointer hand1
link pointer hand2
link pointer pointing_hand

link grab fleur
link grab move
link grab all-scroll
link grabbing closedhand

link wait watch
link wait progress

link not-allowed crossed_circle
link not-allowed no-drop
link not-allowed X_cursor

link crosshair cross
link crosshair tcross

link ns-resize n-resize
link ns-resize s-resize
link ns-resize sb_v_double_arrow
link ns-resize row-resize
link ns-resize v-resize
link ns-resize top_side
link ns-resize bottom_side

link ew-resize e-resize
link ew-resize w-resize
link ew-resize sb_h_double_arrow
link ew-resize col-resize
link ew-resize h-resize
link ew-resize left_side
link ew-resize right_side

link nesw-resize ne-resize
link nesw-resize sw-resize
link nesw-resize size_bdiag
link nesw-resize top_right_corner
link nesw-resize bottom_left_corner

link nwse-resize nw-resize
link nwse-resize se-resize
link nwse-resize size_fdiag
link nwse-resize top_left_corner
link nwse-resize bottom_right_corner

cd "$SWLUI_DIR"

cat > "$OUT_THEME_DIR/index.theme" << 'EOF'
[Icon Theme]
Name=swl
Comment=Tema de cursor do SWL OS (hacker retro: seta clara, contorno ciano)
EOF

echo
echo "Tema gerado em: $OUT_THEME_DIR"
echo "Total de arquivos em cursors/: $(ls "$OUT_THEME_DIR/cursors" | wc -l)"
