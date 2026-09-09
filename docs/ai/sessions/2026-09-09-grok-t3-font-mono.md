# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: T3 — fonte mono / cobertura U+2588 no QEMU

## Escopo

- `apps/tswl/src/render.c` (lista de famílias)
- `userland/build-gui-rootfs.sh` (fonts.conf + cópia do host)

## Problema

No QEMU o glifo █ (U+2588) vira tofu: TSWL pedia só
`JetBrains Mono, Fira Code, monospace` e o rootfs mínimo costuma ter
apenas `DejaVuSans.ttf` (ou nada mono).

## Solução

1. `TSWL_FONT` passa a incluir `DejaVu Sans Mono` e `DejaVu Sans`.
2. `fonts.conf` no rootfs: alias `monospace` → DejaVu Sans Mono/Sans.
3. Se o artefato de build não trouxer Mono/Sans, copia do host
   (`/usr/share/fonts/truetype/dejavu/...`) na hora do `build-gui-rootfs`.

## Teste

Rebuild rootfs/disco, abrir TSWL no QEMU, `swlfetch` ou `printf '\u2588'`.
