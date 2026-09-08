# AFAZERES — Lista-mestra de trabalho

Ordenado por prioridade dentro de cada status. "Desbloqueada" = dá pra
começar agora, sem depender de outra coisa. Owner = quem sugerimos pegar
(ajustar no DIARIO quando mudar).

## 🔓 DESBLOQUEADAS — prontas para começar

| # | Tarefa | Por quê agora | Owner sugerido |
|---|---|---|---|
| A1 | **Corrigir bugs críticos do TSWL** (corrupção de memória via `CSI r`; null deref do `xkb_ctx` no startup; CSI sem clamp → DoS). Ver `docs/revisao/2026-09-05-revisao-01.md` R-01/R-02/R-03 | CONCLUÍDA em 2026-09-08: Grok corrigiu (sessão no repo), orquestrador verificou com build + harness ASan/UBSan (antigo crashava, novo passa). R-01/R-02/R-03 → CORRIGIDO. | — |
| A2 | **Corrigir bugs médios do swl-ui** (hit-test de decoração com minimizada/montada; foco da taskbar após minimizar/fechar; `read_cpu_usage` com variável não inicializada; `mem_available` ausente). Ver R-04/R-05/R-06/R-07 | CONCLUÍDA em 2026-09-08: Grok corrigiu R-04/R-05/R-07 (sessão no repo) e R-06 (double-buffer do shm no TSWL — sessão `2026-09-08-grok-tswl-r06-double-buffer.md`); orquestrador verificou com build + harness e leitura. R-04/R-05/R-06/R-07 → CORRIGIDO. | — |
| B1 | **Ícones PNG do desktop caem no glifo vetorial** — caso encerrado no código atual (2026-09-08, Claude): `desktop.c` tem `resolve_icon_path()` + 13 PNGs em `swl-ui/assets/icons/` (ver sessão `2026-09-08-claude-assets-visuais.md`). | — | — |
| B2 | **Reset do cursor de resize** (`else → "default"` em `swlwm.c`) — presente no código atual (`process_cursor_motion`); bug do "gruda" permanece fechado desde 2026-09-07. Não re-remover. | — | — |
| B3 | **`fetch-deps.sh` gera bash/busybox do host (x86_64)** — num clone limpo em máquina 64-bit, `/bin/sh` nasce quebrado de novo (ver sessão 2026-09-07 §3). Precisa busybox i386 estático reproduzível (ex.: `gcc -m32` com checagem, ou binário pinado). | CONCLUÍDA em 2026-09-08: Grok refez o script (`-m32 -static` fora de host i386, `require_i386_cc` fail-fast, `assert_elf32` via readelf/file, `ensure_elf32_or_rebuild`, `--host=i386-pc-linux-gnu` no bash, flags i386 no busybox, validação final). Orquestrador verificou: `bash -n` OK, funções testadas isoladamente (require/assert/ensure nos 3 caminhos), build do swl-ui com o resto do repo OK. Nota: `ensure_elf32_or_rebuild` tem detalhe de locale (grep `Class:` vs readelf pt_BR `Classe:` + `elif` inalcançável) que causa rebuild redundante em sistema pt_BR — sem impacto no resultado (nenhum 64-bit chega ao rootfs). Correção futura de 1 linha (`LC_ALL=C` + `if` separado). Sessão no repo. | — |
| A3 | **Remover artefatos de build commitados do git** — SUPERADA em 2026-09-07: o histórico foi recomeçado limpo (só fontes; tag `main-arquivo` guarda o antigo com kernel/rootfs). Nada a fazer. | — | — |
| A4 | **Integrar a GUI ao boot real (DRM/KMS)** — CONCLUÍDA em 2026-09-07: swlwm como sessão principal no initramfs, verificado headless e pelo usuário. | — | — |
| A5 | **Readaptar janelas maximizadas quando o output redimensiona** | CONCLUÍDA em 2026-09-08: Grok extraiu `toplevel_apply_maximized_layout()` e reaplica em `output_request_state` + `server_new_output` (`maximized && !minimized`, `saved_geo` intacto). Orquestrador verificou: build OK sem warnings novos, B2 intacto, lógica conferida. Sessão no repo. | — |
| A6 | **Fullscreen real** (hoje só responde ao protocolo, sem lógica) | PARCIAL em 2026-09-08: Grok entregou fullscreen funcional (flag, layout, salva/restaura, exclusão com maximize — build OK, sem warnings novos) e foi integrada; **falta repor** o reflow do `server_new_output` do A5 (arquivo veio de base pré-A5) estendido p/ fullscreen — ver `docs/revisao/2026-09-08-aviso-grok-a6.md` (PENDENTE). | — |
| A7 | **Correções de build/leveza** (tswl: remover `-lm`/`-lrt` sem uso; swl-ui: remover `wayland-protocols` não usada, ordenar wlroots 0.18 antes de 0.19 no fallback, remover `xdg-shell-protocol.c` morto de 72KB) — ver R-09/R-10 | PARCIAL em 2026-09-08: Grok fez R-09/R-10 (sessão no repo; orquestrador verificou com build + `readelf` + teste do fallback; R-09/R-10 → CORRIGIDO, arquivos mortos deletados). **R-11 segue aberto** (bug do parser ANSI, separado — não é build/leveza). | — |
| A8 | **Atualizar `swl-ui/README.md`** (diz que menu e maximizar/minimizar "não existem", mas já estão implementados) — ver R-12 | CONCLUÍDA: R-12 CORRIGIDO desde 2026-09-05; README atual já lista menu/maximizar/minimizar como implementados e a seção de TODO reescrita (conferido em 2026-09-08). | — |

## 🚧 EM CURSO / AGUARDANDO

| # | Tarefa | Estado | Nota |
|---|---|---|---|
| — | `.gitignore` | Corrigido: foi subido como `gitignore` (sem ponto) e o orquestrador renomeou para `.gitignore` em 2026-09-05. | Falta A3 (`git rm --cached` dos já rastreados) |
| — | Deps externas | `userland/fetch-deps.sh` criado por outra IA (kernel + bash/busybox estáticos). | Não commitar a árvore do kernel no futuro |
| — | Documento mestre / estado | Admin edita (esta pasta + `docs/ai/*`) | Contínuo |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |
| — | R-06 (TSWL shm double-buffer) | CONCLUÍDA em 2026-09-08 — fechou o A2. Ver A2. | R-06 → CORRIGIDO |

## 🧭 PLANEJADAS (próximas fases — não bloqueadas por nada, só por ordem)

1. **Linguagem SWL / compilador `swlc`** — MVP–M7 ENTREGUE em
   2026-09-07 (notebook) e verificado pelo admin (46/46). Restos
   possíveis em `swl-compiler/ROADMAP-V1-REMAINING.md`.
2. **Restante do catálogo de apps** (SWLPad, file manager, SEBRE, config…)
   — hoje só placeholders no `desktop.c`; TSWL é o padrão de arquitetura.
3. Instalar/integrar o TSWL de verdade no catálogo (PATH + instalação),
   em vez do comando quebrado atual.
4. TSWL iterações: clipboard/seleção, cores 256, restore de alt-screen,
   scrollback não zerar no resize, modo application-cursor (DECCKM).
5. Testes de regressão — ainda não existe infra de teste no repo; primeiro
   passo razoável é unit test do parser ANSI (ele é o maior risco hoje).
6. Fases longas do roadmap: Electronics Studio, Game/Pixel/3D, browser.

## ⚠️ REGRA PARA QUEM PEGAR TAREFA

Ao abrir uma tarefa da lista: escreva a sessão em `docs/ai/sessions/`,
mexa **só** no escopo da tarefa, rode os testes/montagens relevantes e
marque no DIARIO (não edite este arquivo — quem move aqui é o admin).

---

Histórico de movimentação: este arquivo vive em `docs/orquestracao/` e é
atualizado pelos orquestradores. Conferir sempre o repo real (`git
status`, `git log`) antes de confiar na lista.