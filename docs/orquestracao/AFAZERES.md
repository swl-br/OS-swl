# AFAZERES — Lista-mestra de trabalho

Ordenado por prioridade dentro de cada status. "Desbloqueada" = dá pra
começar agora, sem depender de outra coisa. Owner = quem sugerimos pegar
(ajustar no DIARIO quando mudar).

## 🔓 DESBLOQUEADAS — prontas para começar

| # | Tarefa | Por quê agora | Owner sugerido |
|---|---|---|---|
| A1 | **Corrigir bugs críticos do TSWL** (corrupção de memória via `CSI r`; null deref do `xkb_ctx` no startup; CSI sem clamp → DoS). Ver `docs/revisao/2026-09-05-revisao-01.md` R-01/R-02/R-03 | É entrada não-confiável (saída de qualquer programa) — risco de segurança real. | Quem pegar `apps/tswl` (OpenHands tem o contexto da criação) |
| A2 | **Corrigir bugs médios do swl-ui** (hit-test de decoração com minimizada/montada; foco da taskbar após minimizar/fechar; `read_cpu_usage` com variável não inicializada; `mem_available` ausente). Ver R-04/R-05/R-06/R-07 | Afetam uso real da GUI; nenhum exige decisão de arquitetura. | Quem pegar `swl-ui` (Claude criou menu/maximizar; núcleo é pré-sessões) |
| A3 | **Remover artefatos de build commitados do git** — o `.gitignore` já está corrigido/ativo (estava subido como `gitignore`, sem ponto; renomeado em 2026-09-05) e `userland/fetch-deps.sh` já cobre as deps externas. Falta: `git rm --cached` do que já está rastreado (`build/*`, `rootfs/*`, árvore do kernel). Lista + procedimento em R-08 | Higiene do repo exigida pelo README §20; repo tem 95k arquivos / ~1.8GB de árvore de kernel. | Qualquer IA com acesso ao git (ou o admin, como tarefa de organização) |
| A4 | **Integrar a GUI ao boot real (DRM/KMS)** — rodar o swlwm como sessão gráfica principal dentro do initramfs, sem X11. É o "próximo passo grande" já apontado no PROJECT_STATE | Fase é a que mais destrava valor do projeto: a GUI deixa de rodar "só no host do dev". | IA com contexto de wlroots/kernel (sugerido: construir sobre sessões 0.17/0.18) |
| A5 | **Readaptar janelas maximizadas quando o output redimensiona** | Bug conhecido documentado desde a sessão de maximizar/minimizar. | swl-ui |
| A6 | **Fullscreen real** (hoje só responde ao protocolo, sem lógica) | Pequeno, independente. | swl-ui |
| A7 | **Correções de build/leveza** (tswl: remover `-lm`/`-lrt` sem uso; swl-ui: remover `wayland-protocols` não usada, ordenar wlroots 0.18 antes de 0.19 no fallback, remover `xdg-shell-protocol.c` morto de 72KB) — ver R-09/R-10/R-11 | README §20 obriga dependência só com uso real. | swl-ui / tswl |
| A8 | **Atualizar `swl-ui/README.md`** (diz que menu e maximizar/minimizar "não existem", mas já estão implementados) — ver R-12 | Documentação divergindo do código confunde as IAs seguintes (é uma causa conhecida de retrabalho no projeto). | Admin (docs) |

## 🚧 EM CURSO / AGUARDANDO

| # | Tarefa | Estado | Nota |
|---|---|---|---|
| A9 | **Corrigir R-15/R-16 da camada de boot** — `/dev` não montado no init e **lacuna de build do initramfs** (não dá pra gerar `initramfs.cpio.gz` do zero). R-16 bloqueia boot de clone limpo. Ver `docs/revisao/2026-09-05-revisao-02.md` | ABERTO · desbloqueada | A2 e A1 ficaram para OpenHands; A9/R-15/R-16 pode pegar qualquer IA do boot/userland |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |
| — | Sessões de implementação 09-05 | Usuário ainda não subiu documentos de sessão do dia | A gente atualiza este arquivo quando subirem |

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

Ao abrir uma tarefa da lista: escreva a sessão em `docs/ai/sessions/`,
mexa **só** no escopo da tarefa, rode os testes/montagens relevantes e
marque no DIARIO (não edite este arquivo — quem move aqui é o admin).

---

Histórico de movimentação: este arquivo vive em `docs/orquestracao/` e é
atualizado pelos orquestradores. Conferir sempre o repo real (`git
status`, `git log`) antes de confiar na lista.