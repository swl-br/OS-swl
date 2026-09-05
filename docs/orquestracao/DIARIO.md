# DIARIO — Mudanças e atualizações

Registro diário das rodadas de orquestração. Entrada mais recente em
cima. **Conferir sempre o repo real** — este diário é um índice, não a
verdade de fonte.

## 2026-09-05 — Rodada 3 (admin): processo de docs — sessão só no final; EM_ANDAMENTO

DEC-009 criado em `docs/ai/DECISIONS.md`:
- Sessão (`docs/ai/sessions/`) só no **final**, quando o usuário PEDIR
  (pedaço passou nos testes e está funcionando — não precisa ser 100%).
- Trabalho "no off" (em progresso/teste/quebrado) registrado em
  `docs/ai/EM_ANDAMENTO.md` **assim que começa** — evita colisão de IAs
  (ex.: A4 começado por Claude e OpenHands ao mesmo tempo).
- Formato de sessão vira "relato completo do processo": método + erros
  (na IA / terminal do usuário) + ideias tentadas/descartadas +
  conclusão do que deu certo + tudo que foi implementado.

Registro de andamento criado: `docs/ai/EM_ANDAMENTO.md`.

Situação do A4 registrada lá: **Claude** validou a GUI subindo no boot
numa versão ANTIGA do repo, está reaplicando no estado atual (método
confirmado); **OpenHands** começou a mesma coisa sem saber, parou no
meio (bug, sem progresso). **Decisão do usuário: seguir o método do
Claude.** OpenHands fora de A4 até destravar o bug de parar.

`AFAZERES.md` atualizado: A4 marcado como em andamento; regra de quem
pegar tarefa agora manda registrar no `EM_ANDAMENTO` antes de mergulhar
e só criar sessão no final sob pedido.

## 2026-09-05 — Rodada 2 (admin): revisão da camada de boot + scripts

Estado do repo confirmado (clean, up to date com `origin/main`).

- **`docs/revisao/2026-09-05-revisao-02.md`** criado — revisão da camada
  de boot (assembly) + scripts de build/deps. R-15..R-20.
- **`docs/ai/PROJECT_STATE.md`** atualizado — links da revisão 02,
  seção "Repositório (higiene)" com correção do `.gitignore`, e nova
  seção "Lacunas de build conhecidas".
- **`docs/orquestracao/AFAZERES.md`** atualizado — adicionado A9
  (R-15/R-16 da camada de boot).

Principais descobertas da revisão 02:
- **R-15 [ALTO]**: `init.asm` não monta `/dev` (devtmpfs) — shell/apps
  sem garantia de `/dev/tty*`.
- **R-16 [MÉDIO]**: **nenhum script gera `initramfs.cpio.gz`** — um
  clone do zero + `fetch-deps` + `make` não reproduz o boot. Bloqueador
  de build limpo.
- R-17: limite de ~16MB do INT15h AH=87h (documentar/tratar no futuro).
- R-18: `console=ttyS0` fixo — rever na fase GUI (A4).
- R-19/R-20: refinamentos de `fetch-deps.sh` e `init.asm` (baixa
  prioridade; alguns dependem de A4).

Rodada anterior (Rodada 1) já tinha subido e sido publicada via push
pelo usuário (2 commits: docs de orquestração + limpeza do repo). A
limpeza (untrack do kernel/build/rootfs) está no remoto desde então.

Nenhum código foi alterado (regra do orquestrador).

## 2026-09-05 — Rodada 1 (admin)

Estado do repo conferido na prática: clone íntegro (95.222 arquivos),
index do git reconstruído após checkout interrompido, `git status` limpo.

Entregas desta rodada:

- **`docs/orquestracao/`** criado (README, AFAZERES, IDEIAS, DIARIO) —
  área exclusiva dos orquestradores (eu + GPT).
- **`docs/revisao/2026-09-05-revisao-01.md`** criado — R-01..R-14
  (swl-ui + tswl), com responsáveis e pedido de correção.
- **`docs/ai/PROJECT_STATE.md`** atualizado — seções "Repositório
  (higiene)", "Revisões / QA" e "Espaço de orquestração".
- **Correção do `.gitignore`**: outra IA subiu `gitignore` (sem ponto);
  o orquestrador renomeou para `.gitignore` (senão não funciona).
- **`docs/ai/DECISIONS.md`** não precisou de mudança nesta rodada.

Descobertas que viraram tarefas em `AFAZERES.md`:

- A1: 3 bugs críticos no TSWL (parser ANSI) — R-01 a R-03 → OpenHands.
- A2: 4 bugs médios no swl-ui — R-04 a R-07.
- A3: limpeza manual de artefatos já commitados — **FEITA** na Rodada 1
  (2º commit, `26c3f3feb`, untrack de build/rootfs/kernel).
- A4: integração GUI ao boot real (DRM/KMS) — grande, desbloqueada.
- A5/A6: bugs conhecidos da GUI (maximize/resize, fullscreen).
- A7: leveza de build (dependências mortas, ordem wlroots, arquivo morto).
- A8: `swl-ui/README.md` desatualizado — **CORRIGIDO** por outra IA.

Publicado via push pelo usuário (Rodada 1): commits `e72e57031` e
`26c3f3feb` estão no `origin/main`.

Nenhum código foi alterado (regra: orquestrador não mexe em código por
padrão).