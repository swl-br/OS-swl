# DIARIO — Mudanças e atualizações

Registro diário das rodadas de orquestração. Entrada mais recente em
cima. **Conferir sempre o repo real** — este diário é um índice, não a
verdade de fonte.

## 2026-09-08 — A5 + B3 integradas (admin)

Duas frentes do Grok em `revisao-de-entrada_LOCAL/`, verificadas e
aprovadas pelo usuário:

**A5** (maximizadas no resize do output): `toplevel_apply_maximized_layout()`
extraída e reaplicada em `output_request_state` + `server_new_output`
(`maximized && !minimized`, `saved_geo` intacto). Build `meson`+`ninja`
OK sem warnings novos; B2 intacto. A5 → CONCLUÍDA.

**B3** (`fetch-deps.sh` i386): script refeito (`-m32 -static`,
`require_i386_cc`, `assert_elf32`, `ensure_elf32_or_rebuild`,
`--host=i386-pc-linux-gnu`, flags i386 no busybox, validação final).
Verificação: `bash -n` OK; funções testadas isoladamente (require OK,
assert aceita ELF32 e rejeita 64-bit, ensure nos 3 caminhos); veredito:
**resolve o B3** — nenhum 64-bit chega ao rootfs em nenhum caminho.
Ressalva registrada no AFAZERES: detalhe de locale no `ensure`
(readelf pt_BR imprime `Classe:`, grep espera `Class:`, `elif`
inalcançável) causa rebuild redundante em sistema pt_BR, sem impacto
no resultado; correção futura de 1 linha. B3 → CONCLUÍDA.

## 2026-09-08 — Revisão completa: builds + testes (admin)

Rodada de verificação geral do sistema (builds em `/tmp`, repo intacto):

- `apps/tswl`: `meson`+`ninja` OK, **0 warnings**.
- `apps/swlpad`: `meson`+`ninja` OK, **0 warnings**.
- `swl-ui`: `meson`+`ninja` OK (12/12); 16 warnings — 15×
  `-Wunused-parameter` (benignos, listeners Wayland) + 1×
  `-Wformat-truncation` em `panel.c:204` (`snprintf` do label MEM,
  cosmético: só estouraria com valores absurdos de RAM).
- `swl-compiler`: `make all rt` OK, 0 warnings; `make test` =
  **51 passed, 0 failed** (22 exemplos + 29 rejeições — bate com o
  README). Exemplos cobrem v2 (`for_loop.swl`, `hex_demo.swl`, etc.).
- Harness adversarial do parser TSWL (ASan+UBSan, 21 casos: CSI `r`
  malicioso, 1500 params, SGR/cursor absurdos, resize, OSC/DCS lixo,
  feed de 100KB, UTF-8 quebrado): **limpo, sem sanitizer**.
- Scripts (`fetch-deps.sh`, `build-*.sh`, `runtests.sh`): `bash -n` OK.
- `tools/gen-cursors`: compila com a receita do `generate.sh`
  (`gcc -O2`); com `-std=c11` estrito reclama de `M_PI` — falso alarme,
  receita oficial funciona.

Observações (sem ação, aguardando decisão do usuário):

1. `tswl_term_resize` não valida `cols/rows < 1`. Fora de contrato e
   inalcançável em produção (`cols/rows_for` prendem em ≥1 e o
   `toplevel_configure` exige `w>0 && h>0`; overflow p/ wrap exigiria
   janela > 2^33 px). Endurecimento de 1 linha possível, mas não é bug
   vivo — não virou achado formal.
2. Não testável aqui: `disk.img` completo (precisa da árvore do kernel
   + rootfs, externos), GUI em runtime (precisa de display/compositor)
   e `fetch-deps.sh` de ponta a ponta (baixaria toolchain; B3 segue
   aberto e é o único item desse tipo).

Docs: `AFAZERES.md`/`PROJECT_STATE.md` conferidos contra o repo — sem
divergência restante (abertos reais: A5, A6, A7/R-09..11, R-13, R-14,
B3). Nenhuma doc precisou de correção nesta rodada além deste registro.

## 2026-09-08 — A2 completa: R-06 (TSWL double-buffer) corrigida e integrada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/tswl-R06-files/` a correção
da **R-06** pelo Grok (`apps/tswl/src/main.c` + sessão). Orquestrador
verificou: diff só no `main.c` do tswl (base `baae84b`, pós A1/A2);
build `meson`+`ninja` OK; A1 preservado (`xkb_ctx` guard intacto);
lógica do double-buffer conferida (`busy`/`stale`, recriação no
`release`, nunca destrói buffer em uso, `need_redraw` mantido quando
sem slot livre); zero refs ao antigo buffer único. Aprovado pelo
usuário e integrado: R-06 → CORRIGIDO, A2 COMPLETA.

## 2026-09-08 — Revisão geral + docs sincronizadas (admin)

Revisão geral do estado real (repo é a fonte de verdade) e atualização
das docs que estavam defasadas — nenhum código alterado:

- `PROJECT_STATE.md`: "Revisões/QA" reflete R-01..R-07 e R-12
  corrigidos; "Em investigação" passa B1 (ícones PNG) e B2 (reset do
  cursor) para **encerrados no código atual**; árvore do swl-ui ganhou
  `context_menu.c/h`; próximos passos GUI atualizados (ícones saem).
- `AFAZERES.md`: B1/B2 → encerrados no código; A8 → CONCLUÍDA (R-12
  CORRIGIDO desde 09-05, README conferido). R-06 entrou como em curso
  (Grok) — desde então a R-06 subiu e foi integrada (ver entrada A2 completa acima).

## 2026-09-08 — A2 (swl-ui bugs médios) parcial corrigida e verificada (admin)

Usuário colocou em `revisao-de-entrada_LOCAL/swl-ui-A2-files/` a correção
da **A2** pelo Grok (`swlwm.c` + `panel.c` + sessão). Orquestrador
verificou: base == `HEAD` atual (nada novo tocou swl-ui desde então),
build `meson`+`ninja` (wlroots 0.17.1) OK, lógica R-04 (hit-test pula
minimizadas), R-05 (taskbar usa foco real do teclado; minimizar a focada
passa foco à próxima) e R-07 (contadores zerados + fallback de memória
com clamp) conferidas — harness contra o `/proc` real e teste do fallback
passaram. Aprovado pelo usuário (só sobe o que ajuda) e integrado:
R-04/R-05/R-07 → CORRIGIDO; A2 parcial — **R-06 (TSWL shm) segue em
aberto**.

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