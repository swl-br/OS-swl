# AFAZERES — Lista-mestra de trabalho

Ordenado por prioridade dentro de cada status. "Desbloqueada" = dá pra
começar agora, sem depender de outra coisa. Owner = quem sugerimos pegar
(ajustar no DIARIO quando mudar).

## 🔓 DESBLOQUEADAS — prontas para começar

| # | Tarefa | Por quê agora | Owner sugerido |
|---|---|---|---|
| A1 | **Corrigir bugs críticos do TSWL** (corrupção de memória via `CSI r`; null deref do `xkb_ctx` no startup; CSI sem clamp → DoS). Ver `docs/revisao/2026-09-05-revisao-01.md` R-01/R-02/R-03 | É entrada não-confiável (saída de qualquer programa) — risco de segurança real. | Quem pegar `apps/tswl` (OpenHands tem o contexto da criação) |
| A2 | **Corrigir bugs médios do swl-ui** (hit-test de decoração com minimizada/montada; foco da taskbar após minimizar/fechar; `read_cpu_usage` com variável não inicializada; `mem_available` ausente). Ver R-04/R-05/R-06/R-07 | Afetam uso real da GUI; nenhum exige decisão de arquitetura. | Quem pegar `swl-ui` (Claude criou menu/maximizar; núcleo é pré-sessões) |
| ~~A3~~ | **✅ CONCLUÍDA** (2026-09-05, `26c3f3feb`): artefatos rastreados removidos do git (`build/*`, `rootfs/*`, kernel). `.gitignore` ativo. | — | — |
| A4 | GUI no boot real (DRM/KMS) — **ver seção "EM ANDAMENTO" abaixo** (estado unificado, aguardando teste final do mouse) | — | Claude (método escolhido) |
| A5 | **Readaptar janelas maximizadas quando o output redimensiona** | Bug conhecido documentado desde a sessão de maximizar/minimizar. | swl-ui |
| A6 | **Fullscreen real** (hoje só responde ao protocolo, sem lógica) | Pequeno, independente. | swl-ui |
| A7 | **Correções de build/leveza** (tswl: remover `-lm`/`-lrt` sem uso; swl-ui: remover `wayland-protocols` não usada, ordenar wlroots 0.18 antes de 0.19 no fallback, remover `xdg-shell-protocol.c` morto de 72KB) — ver R-09/R-10/R-11 | README §20 obriga dependência só com uso real. | swl-ui / tswl |
| A8 | **Atualizar `swl-ui/README.md`** (diz que menu e maximizar/minimizar "não existem", mas já estão implementados) — ver R-12 | Documentação divergindo do código confunde as IAs seguintes (é uma causa conhecida de retrabalho no projeto). | Admin (docs) |

## 🚧 EM CURSO / AGUARDANDO

| # | Tarefa | Estado | Nota |
|---|---|---|---|
| A9 | **R-16: receita de `initramfs.cpio.gz` — ✅ CONCLUÍDA (2026-09-06)**: `userland/build-initramfs.sh` + dependência no `Makefile`. Verificada (cpio válido, com GUI). *Nota: `bzImage` continua build manual por design.* | ✅ | — |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |

## 🔄 EM ANDAMENTO / ESTADO UNIFICADO (2026-09-06)

| # | O que | Quem | Acompanhar em |
|---|---|---|---|
| A4 | GUI no boot (DRM/KMS) — método do Claude **juntado no repo** (init.asm GUI-first, scripts gui-i386/rootfs, swlpad, resize+mouse). **Aguardando teste final do usuário (mouse)** | Claude | `docs/ai/EM_ANDAMENTO.md`; quando validar → sessão |
| — | Receita do initramfs (R-16) | **CONCLUÍDA** (admin, 2026-09-06) | review 02 |
| — | Sessões de implementação 09-05/06 | Usuário ainda não subiu (padrão DEC-009: só no final) | — |

## 🧭 PLANEJADAS (próximas fases — não bloqueadas por nada, só por ordem)

1. **Linguagem SWL / compilador `swlc`** — fase inteira ainda não iniciada;
   caminho técnico já definido no README (lexer → parser → AST → backend).
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

Ao abrir uma tarefa da lista: registre em `docs/ai/EM_ANDAMENTO.md`
(assim que começar — mesmo que incompleto/no off) e mexa **só** no
escopo da tarefa, rode os testes/montagens relevantes e marque no DIARIO
(não edite este arquivo — quem move aqui é o admin). A **sessão
completa** (`docs/ai/sessions/`) só é criada **no final**
quando o usuário pedir (trabalho passou nos testes e está funcionando).
Ver DEC-009.

---

Histórico de movimentação: este arquivo vive em `docs/orquestracao/` e é
atualizado pelos orquestradores. Conferir sempre o repo real (`git
status`, `git log`) antes de confiar na lista.