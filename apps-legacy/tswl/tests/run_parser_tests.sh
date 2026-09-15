#!/bin/sh
# Roda os unit tests do parser ANSI sem dependências Wayland/Cairo.
set -eu
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c11 -Wall -Wextra -O0 -g}"
OUT="${TMPDIR:-/tmp}/tswl_test_term_parser_$$"
# Sanitizers opcionais (orquestrador usa ASan/UBSan no harness)
if [ "${TSWL_SANITIZE:-0}" = "1" ]; then
  CFLAGS="$CFLAGS -fsanitize=address,undefined"
fi
$CC $CFLAGS -I"$ROOT/include" -o "$OUT" test_term_parser.c "$ROOT/src/term.c"
"$OUT"
ec=$?
rm -f "$OUT"
exit $ec
