# Sessao — 2026-09-11

IA: Grok
Base: origin/main (bracketed paste + W1 + select_all + bell)

## CSI 3 J
Limpa tela + zera scrollback (back_count/head/offset).

## CSI !p (DECSTR soft reset)
- attrs/cores default
- scroll region full
- app_cursor off
- bracketed_paste off
- cursor visible

Intermediate CSI trackado (csi_intermed).

Testes: test_csi_3j_scrollback + test_soft_reset.
