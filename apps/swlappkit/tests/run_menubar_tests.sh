#!/bin/sh
# Roda os unit tests do componente menubar (M1) sem dependencias Wayland.
set -eu
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c11 -Wall -Wextra -O0 -g}"
PKG="$(pkg-config --cflags --libs pangocairo)"
OUT="${TMPDIR:-/tmp}/swlappkit_test_menubar_$$"
$CC $CFLAGS -I"$ROOT/include" -o "$OUT" test_menubar.c "$ROOT/src/menubar.c" $PKG
"$OUT"
ec=$?
rm -f "$OUT"
exit $ec
