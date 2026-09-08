# DIARIO — Mudanças e atualizações

Registro diário das rodadas de orquestração. Entrada mais recente em
cima. **Conferir sempre o repo real** — este diário é um índice, não a
verdade de fonte.

## 2026-09-08 — A1 (TSWL bugs críticos) corrigida e verificada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/tswl-A1-files/` a correção
da **A1** pelo Grok (`term.c` + `main.c` + sessão). Orquestrador:
compilou o tswl com os fontes novos (`meson`+`ninja`, OK) e rodou um
harness de parser ANSI com ASan/UBSan — o `term.c` antigo reproduzia o
crash R-01 (`negative-size-param` no `memmove`), o novo passa limpo em
todas as sequências maliciosas (R-01/R-03) sem UB. R-02 confirmado por
leitura (xkb_ctx antes do primeiro roundtrip + guard). Aprovado pelo
usuário e integrado: R-01/R-02/R-03 → CORRIGIDO, A1 concluída.

## 2026-09-08 — swlc v1 reconciliado + verificado (admin)

Notebook subiu `swl-compiler/` (MVP–M7) + sessão de fechamento.
Admin: FF limpo, WIP local intacto, `make -C swl-compiler test`
rodado aqui → **46 passed, 0 failed** (entrega confirmada de forma
independente). Docs: `PROJECT_STATE` (swlc) + `AFAZERES` (fase 1
entregue, ver `ROADMAP-V1-REMAINING.md` pros restos).

## 2026-09-08 — Notebook reconciliado (admin)

Usuário subiu via web o trabalho do notebook (`origin/main` 93071b7 +
branch `origin/arquivos`): wallpaper novo (1,2 MB), `swlcat.png`,
`swl_desktop_draw_glyph_for_title()` (ícones da taskbar por keyword),
`SWL_MIN_WINDOW_*` + throttle de resize (`resize_deco_dirty`, redraw
por frame). FF do `main` local + merge do WIP local (term.c, desktop.c,
swlwm.c) SEM conflitos — regiões disjuntas. B2 segue aberto (reset do
cursor ainda fora). `swlcat.png` sem referência (asset futuro).

## 2026-09-07 — Rodada 2 (admin)

Pasta oficial apagada pelo usuário no meio do dia; reconstruída dos
snapshots + histórico, com 4 fixes de boot no caminho (initrd no topo
da RAM, `/bin/sh`→busybox i386, xkb-data+`/dev/shm`, `devpts` pro
tswl). GUI verificada ponta a ponta (headless+usuário) e subida:
`main` recomeçado limpo (só fontes), tag `main-arquivo` guarda o
histórico antigo. Ver `docs/ai/sessions/2026-09-07-recuperacao-pasta-boot-gui.md`.

Docs atualizadas nesta rodada: `PROJECT_STATE.md` (estado real),
`AFAZERES.md` (A3/A4 concluídas; novas B1/B2/B3), `swl-ui/README.md`
(A8: menu/maximizar/PNG não são mais TODO).

Em curso por outras IAs (não mexer no código delas): PNG dos ícones
(B1), resize do terminal (term.c), compat wlroots (`WLR_BUTTON_`
→ `WL_POINTER_BUTTON_STATE_`). ATENÇÃO B2: removeram o reset do
cursor de resize (`else → "default"` em `swlwm.c`) — reabre bug
confirmado pelo usuário; reaplicar antes de fechar.

Nenhum código alterado pelo admin nesta rodada (só docs).

## 2026-09-05 — Rodada 1 (admin)

Estado do repo conferido na prática: clone íntegro (95.222 arquivos),
index do git reconstruído após checkout interrompido, `git status`
limpo.

Entregas desta rodada:

- **`docs/orquestracao/`** criado (README, AFAZERES, IDEIAS, DIARIO) —
  área exclusiva dos orquestradores (eu + GPT).
- **`docs/revisao/2026-09-05-revisao-01.md`** criado — 12 achados da
  revisão de `swl-ui` + `apps/tswl`, com responsáveis e pedido de
  correção.
- **`docs/ai/PROJECT_STATE.md`** atualizado — novas seções:
  "Repositório (higiene)" e "Revisões / QA", com links para a revisão.
- **`docs/ai/DECISIONS.md`** não precisou de mudança nesta rodada.

Descobertas que viraram tarefas em `AFAZERES.md`:

- A1: 3 bugs críticos no TSWL (parser ANSI) — R-01 a R-03.
- A2: 4 bugs médios no swl-ui — R-04 a R-07.
- A3: limpeza manual de artefatos já commitados (`.gitignore` foi criado
  por outra IA e subido, mas não remove o que já está rastreado).
- A4: integração GUI ao boot real (DRM/KMS) — grande, desbloqueada.
- A5/A6: bugs conhecidos da GUI (maximize/resize, fullscreen).
- A7: leveza de build (dependências mortas, ordem wlroots, arquivo morto).
- A8: `swl-ui/README.md` desatualizado.

Processo: houve sessão do usuário em paralelo (ids não subidos ainda).
Quando subirem, atualizar `AFAZERES.md` se algo mudar de status.

Nenhum código foi alterado nesta rodada (regra: orquestrador não mexe em
código por padrão).