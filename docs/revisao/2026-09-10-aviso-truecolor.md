# AVISO — truecolor: somar com OSC, não trocar (2026-09-10, orquestrador)

## Veredito: LÓGICA APROVADA (25/25), integração BLOQUEADA por base velha

`rgb_to_256` + SGR 38;2/48;2 verificados e corretos. Mas o `term.c`
entregue não tem OSC (`osc_buf`, `take_title`) e o teste **substitui**
o do OSC em vez de somar — subir apagaria o título da janela e sua
cobertura.

## O que fazer (sobre o term.c atual com OSC+256)

1. Somar `rgb_to_256` + ramos 38;2/48;2 no SGR (regiões disjuntas do
   OSC — sem conflito).
2. Teste `test_sgr_truecolor` ADICIONADO junto ao `test_osc_title`
   (suíte deve ir a 30, não voltar a 25).

Status: ATENDIDO em 2026-09-10 — rebase somando (30/30 no repo) integrado.
