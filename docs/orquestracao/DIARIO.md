# DIARIO — Mudanças e atualizações

Registro diário das rodadas de orquestração. Entrada mais recente em
cima. **Conferir sempre o repo real** — este diário é um índice, não a
verdade de fonte.

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