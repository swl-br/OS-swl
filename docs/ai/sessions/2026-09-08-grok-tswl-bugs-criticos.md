# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: A1 — corrigir bugs críticos do TSWL (R-01, R-02, R-03)

## Escopo

Apenas `apps/tswl/src/term.c` e `apps/tswl/src/main.c`.
Nenhuma alteração em swl-ui, boot, userland ou documentação de estado
(PROJECT_STATE / AFAZERES ficam para o orquestrador).

## Problemas corrigidos

### R-01 — [CRÍTICO] Corrupção de memória via CSI r

**Antes:** `CSI Pt;Pb r` só limitava `scroll_top < 0` e `scroll_bot >= rows`.
`ESC[9999;1r` deixava `scroll_top > scroll_bot`. Um `CSI S`/`T` ou
newline seguinte calculava `(scroll_bot - scroll_top)` negativo →
`size_t` gigante no `memmove` → leitura/escrita fora do grid.

**Depois:**
- Clamp individual de top/bot para `[0, rows-1]`.
- Se `scroll_top > scroll_bot` após o clamp → região vira tela inteira
  (`0 .. rows-1`), como terminais reais fazem com região inválida.
- Defesa em profundidade em `scroll_up` / `scroll_down`: se a região
  ainda estiver invertida, retornam imediatamente (no-op) e o tamanho
  do `memmove` é derivado de `span = bot - top` (sempre ≥ 0).

### R-02 — [ALTO] Null deref no startup (xkb_ctx)

**Antes:** `xkb_context_new` era chamado *depois* do segundo
`wl_display_roundtrip` (configure). O primeiro roundtrip (após bind do
registry) já pode entregar `wl_seat.capabilities` → `get_keyboard` →
`wl_keyboard.keymap`, e o listener chamava
`xkb_keymap_new_from_string(a->xkb_ctx, …)` com `xkb_ctx == NULL`.

**Depois:**
- `xkb_context_new` moveu para logo após `wl_display_connect`,
  **antes** do primeiro roundtrip.
- Guard adicional em `keyboard_keymap`: se `xkb_ctx` for NULL, fecha o
  fd e retorna em vez de null-deref.

### R-03 — [ALTO] CSI sem clamp (DoS / UB)

**Antes:** dígitos CSI acumulavam sem limite (`int` overflow = UB);
`CSI 999999 L/M/S/T` rodava milhões de `memmove`.

**Depois:**
- Constante `CSI_PARAM_MAX 9999`.
- Parsing de dígitos para de crescer ao atingir o limite
  (`*p <= CSI_PARAM_MAX/10` antes de `*10 + d`).
- `param()` clampa valores negativos (overflow residual) e acima do
  máximo.
- `L`, `M`, `S`, `T` limitam a contagem ao tamanho da região de scroll
  (`scroll_bot - scroll_top + 1`).

## Arquivos alterados

- `apps/tswl/src/term.c` — parser CSI, scroll region, scroll_up/down
- `apps/tswl/src/main.c` — ordem de criação do xkb_ctx + guard

## O que NÃO foi feito nesta sessão

- R-06 (double-buffer / wait release do shm) — médio, fora de A1.
- R-09 (remover `-lm`/`-lrt` do meson) — baixo, A7.
- Testes automatizados do parser (ainda não existe infra).
- Commit / push (regra do usuário: não fazer automaticamente).

## Verificação

Ambiente de sandbox sem meson/ninja nem deps Wayland/cairo completas,
então não foi possível `ninja -C build` aqui. As alterações são
localizadas e foram conferidas por leitura estática:

- Todos os pontos apontados pela revisão 2026-09-05 foram endereçados.
- Nenhuma API pública de `term.h` mudou.
- Comportamento normal (região válida, contagens pequenas, startup
  com teclado) permanece o mesmo; só os caminhos degenerados foram
  endurecidos.

Sugestão de teste manual no host de desenvolvimento:

```bash
cd apps/tswl && meson setup build && ninja -C build
# sob o compositor:
./build/tswl
# em outro terminal / ou printf no PTY:
printf '\033[9999;1r\033[5S'   # antes crashava / corrompia; agora no-op seguro
printf '\033[999999999A'       # cursor sobe no máximo o necessário
```

## Próximo passo sugerido

- Orquestrador: marcar R-01/R-02/R-03 como resolvidos na revisão e
  atualizar AFAZERES (A1).
- Quem tiver ambiente gráfico: validar interativamente o TSWL após o
  build.
- A2 (bugs médios do swl-ui) ou A3 (higiene do repo) são os candidatos
  naturais a seguir.
