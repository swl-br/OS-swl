# Sessão GENÉRICA de orientação — estado unificado (2026-09-06)

> Esta sessão é especial: **não relata um trabalho específico** — ela
> orienta qualquer IA que entre no projeto a não se perder. Várias
> sessões de trabalho dos dias 04-06/09 **foram perdidas pelo usuário**
> (não foram subidas). O que está descrito aqui é o que se CONSEGUIU
> reconstruir pelo repositório/`DIARIO`/`EM_ANDAMENTO`. Use como ponto
> de partida — e SEMPRE confira o repo real (DEC-008).

---

## 1. Onde o projeto VIVE (paSTA única oficial)

- **Pasta única oficial**: `~/Documentos/SWL-OS` (renomeada de
  `Default Project` em 2026-09-06).
- Repo git conectado a `github.com/swl-br/OS-swl`, branch `main`.
  Para push/pull de verdade, quem publica é o usuário ou o admin.
- NÃO trabalhar em: `~/Documentos/Dev/OS-swl` (cópia de trabalho do
  Claude, sem `.git` — pode divergir), nem `~/Documentos/Estudos/*`
  (aquele `.git` é do repo de backup `swl-maycon/beckup`, outra conta —
  NÃO é o projeto).
- `gui-artifacts/`, `rootfs/`, `build/`, `kernel/linux-7.2.1/`,
  `apps/*/build/` são **artefatos locais gitignored** — não vão pro git.

## 2. Estado atual (resumo — detalhes em PROJECT_STATE/AFAZERES/DECISIONS)

- **Boot**: funcional ponta a ponta em QEMU (bootloader próprio +
  kernel Linux 7.2.1 32-bit + initramfs + init).
- **GUI (A4)**: método do Claude **validado** (GUI subiu no boot numa
  base antiga) e **juntado** no repo (scripts `build-gui-i386.sh` +
  `build-gui-rootfs.sh`, `init.asm` GUI-first com fallback pro shell).
  **Resize com mouse** implementado (`swlwm.c` + `theme.h`,
  `SWL_RESIZE_MARGIN`). **Aguardando teste final do usuário** — então
  A4 ainda NÃO está 100% fechado.
- **R-15 corrigido**: `init.asm` monta `devtmpfs` em `/dev` e `tmpfs`
  em `/run` antes de subir GUI/shell.
- **R-16 corrigido (2026-09-06)**: `userland/build-initramfs.sh` +
  `Makefile` geram `build/initramfs.cpio.gz` a partir do `rootfs/`.
  Kernel (`bzImage`) continua build manual por design
  (`fetch-deps.sh` → `make -C kernel/linux-7.2.1`).
- **App novo**: `apps/swlpad/` (fonte; build local gitignored).

## 3. Trabalhos que ficaram SEM sessão (perdidas) — reconstrução

Realizado no período, comprovado por código/`DIARIO`, mas sem sessão
individual (as originais se perderam):

| O quê | Onde | Evidência |
|---|---|---|
| Resize de janela com mouse (cursor + bordas) | `swl-ui/src/swlwm.c`, `swl-ui/include/theme.h` | código presente |
| GUI no boot (A4) — método/roteiro | `scripts/build-gui-i386.sh`, `userland/build-gui-rootfs.sh`, `Makefile` (`run-gui`) | scripts presentes |
| Mount de /dev (devtmpfs) e /run no init | `userland/init.asm` | código presente (R-15) |
| App SWLPad (esqueleto funcional) | `apps/swlpad/` | fonte presente |
| Integração swlwm predumeri (wlroots 0.18) | `swl-ui` (guard `SWL_WLR_0_18`) | DEC-006, PROJECT_STATE |

Documentos mestre: `docs/ai/PROJECT_STATE.md` (estado real),
`docs/ai/DECISIONS.md`, `docs/orquestracao/AFAZERES.md` (lista de
tarefas), `docs/orquestracao/DIARIO.md` (o que cada rodada fez),
`docs/ai/EM_ANDAMENTO.md` (o que está EM progresso agora, ainda não
fechado).

## 4. Regra de documentação (DEC-009)

- **Sessão** (`docs/ai/sessions/`) só é escrita **no final** de um
  trabalho, **quando o usuário pedir**, e depois de passar nos testes —
  formato "relato completo" (método + erros + ideias tentadas +
  conclusão + tudo implementado).
- **Trabalho em progresso/no off** vai para `docs/ai/EM_ANDAMENTO.md`
  assim que começar. **Se não está lá nem no repo, assume-se que não
  existe.**
- Mapeie SEMPRE o repo real antes de mexer (`git status`, `git log`,
  listagem) — DEC-008. Repo é a fonte de verdade.

## 5. Quem é quem

- **Claude** — GUI (menu, maximizar/minimizar, A4/boot, resize+mouse,
  swlpad). Método do A4 escolhido pelo usuário.
- **OpenHands** — TSWL (terminal), integração swl-ui, docs, fetch-deps.
  Cancelada do A4 (parava no meio); fora até destravar.
- **Admin/orquestrador** (paulista/GPT) — docs de estado, revisões
  (`docs/revisao/`), AFAZERES/DIARIO/EM_ANDAMENTO. Não edita código por
  padrão (mas corrige o que bloquear a versão única funcional).

## 6. Próximos passos imediatos (do AFAZERES)

- A1: bugs críticos TSWL (R-01/R-02/R-03) → quem pegar tswl.
- A2: bugs médios swl-ui (R-04..R-07).
- A4: aguardando o teste final do mouse; depois fechar e marcar.
- R-16 (A9): receita de `initramfs.cpio.gz` no repo.