# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: T1 — cursor barra vertical piscando (fatia de T4)

## Escopo

Apenas `apps/tswl/src/render.c`.

## Problema

Cursor era bloco sólido com texto invertido (`render.c`), usuário
pediu barra `|` piscando no lugar certo (QEMU).

## Solução

- Retângulo fino (~1/8 da célula, 2–3 px) na borda esquerda da célula
- Mesma cor do fg da célula, alpha 0.95
- Blink continua no `main.c` (timer 500 ms, `cursor_on`)
- Sem redesenho invertido do glyph (barra sobreposta)

## Nota

T2 (wheel/scroll QEMU) e T3 (fonte mono) ficam separados.
