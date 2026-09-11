# Sessao — 2026-09-11

IA: Grok
Data: 2026-09-11
Responsavel: TSWL bracketed paste (CSI ?2004)

Base: origin/main (raw, com W1/bell/select_all)

## Comportamento

- CSI `?2004h` / `?2004l` liga/desliga modo
- clipboard_paste envolve com ESC[200~ ... ESC[201~ se ativo
- Shell (readline/bash) nao interpreta metachars no paste

## Preservado

utf8, OSC, 256/truecolor, select_*, take_bell, buf_w
