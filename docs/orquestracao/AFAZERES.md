# AFAZERES — Lista-mestra de trabalho

Ordenado por prioridade dentro de cada status. "Desbloqueada" = dá pra
começar agora, sem depender de outra coisa. Owner = quem sugerimos pegar
(ajustar no DIARIO quando mudar).

## 🔓 DESBLOQUEADAS — prontas para começar

| # | Tarefa | Por quê agora | Owner sugerido |
|---|---|---|---|
| A1 | **Corrigir bugs críticos do TSWL** (corrupção de memória via `CSI r`; null deref do `xkb_ctx` no startup; CSI sem clamp → DoS). Ver `docs/revisao/2026-09-05-revisao-01.md` R-01/R-02/R-03 | É entrada não-confiável (saída de qualquer programa) — risco de segurança real. | Quem pegar `apps/tswl` (OpenHands tem o contexto da criação) |
| A2 | **Corrigir bugs médios do swl-ui** (hit-test de decoração com minimizada/montada; foco da taskbar após minimizar/fechar; `read_cpu_usage` com variável não inicializada; `mem_available` ausente). Ver R-04/R-05/R-06/R-07 | Afetam uso real da GUI; nenhum exige decisão de arquitetura. | Quem pegar `swl-ui` (Claude criou menu/maximizar; núcleo é pré-sessões) |
| B1 | **Ícones PNG do desktop caem no glifo vetorial** (confirmado por IoU 0,04; wallpaper carrega — falha específica). Em investigação por outra IA (paths + `term.c`/resize). Ver `docs/ai/sessions/2026-09-07-recuperacao-pasta-boot-gui.md` | Regressão visual central: desktop deveria mostrar os 13 PNGs. | Quem está em `swl-ui/src/desktop.c` |
| B2 | **REGRESSÃO SINALIZADA: reset do cursor de resize removido** (`swlwm.c`, bloco `else → "default"`). Sem ele a setinha "gruda" dentro da janela (bug confirmado pelo usuário em 2026-09-07 e corrigido; remoção reabre). Reaplicar antes de fechar. | Reabre bug validado pelo usuário. | Quem está em `swl-ui/src/swlwm.c` |
| B3 | **`fetch-deps.sh` gera bash/busybox do host (x86_64)** — num clone limpo em máquina 64-bit, `/bin/sh` nasce quebrado de novo (ver sessão 2026-09-07 §3). Precisa busybox i386 estático reproduzível (ex.: `gcc -m32` com checagem, ou binário pinado). | Quebra o boot de qualquer clone limpo. | Admin ou quem pegar build |
| A3 | **Remover artefatos de build commitados do git** — SUPERADA em 2026-09-07: o histórico foi recomeçado limpo (só fontes; tag `main-arquivo` guarda o antigo com kernel/rootfs). Nada a fazer. | — | — |
| A4 | **Integrar a GUI ao boot real (DRM/KMS)** — CONCLUÍDA em 2026-09-07: swlwm como sessão principal no initramfs, verificado headless e pelo usuário. | — | — |
| A5 | **Readaptar janelas maximizadas quando o output redimensiona** | Bug conhecido documentado desde a sessão de maximizar/minimizar. | swl-ui |
| A6 | **Fullscreen real** (hoje só responde ao protocolo, sem lógica) | Pequeno, independente. | swl-ui |
| A7 | **Correções de build/leveza** (tswl: remover `-lm`/`-lrt` sem uso; swl-ui: remover `wayland-protocols` não usada, ordenar wlroots 0.18 antes de 0.19 no fallback, remover `xdg-shell-protocol.c` morto de 72KB) — ver R-09/R-10/R-11 | README §20 obriga dependência só com uso real. | swl-ui / tswl |
| A8 | **Atualizar `swl-ui/README.md`** (diz que menu e maximizar/minimizar "não existem", mas já estão implementados) — ver R-12 | Documentação divergindo do código confunde as IAs seguintes (é uma causa conhecida de retrabalho no projeto). | Admin (docs) |

## 🚧 EM CURSO / AGUARDANDO

| # | Tarefa | Estado | Nota |
|---|---|---|---|
| — | `.gitignore` | Corrigido: foi subido como `gitignore` (sem ponto) e o orquestrador renomeou para `.gitignore` em 2026-09-05. | Falta A3 (`git rm --cached` dos já rastreados) |
| — | Deps externas | `userland/fetch-deps.sh` criado por outra IA (kernel + bash/busybox estáticos). | Não commitar a árvore do kernel no futuro |
| — | Documento mestre / estado | Admin edita (esta pasta + `docs/ai/*`) | Contínuo |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |

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