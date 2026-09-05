# IDEIAS — Propostas e pensamentos soltos

Ideias que ainda não viraram tarefa em `AFAZERES.md`. Quando uma virar
tarefa, mover para lá (com os prós/contras e decisão). Nada aqui é
compromisso.

## Infraestrutura e build

- **Kernel como submodule ou script de fetch** — tirar a árvore de
  1.8GB do repo principal (README §20 já pede; decisão de forma:
  submodule vs. script `fetch-kernel.sh` + hash fixo).
- **`.gitignore` mais robusto** — além de `build/`, cobrir `apps/*/build`,
  `swl-ui/build`, `rootfs/` binários, `*.log`, `.ninja_*`, `meson-info`.
- **Pipeline "build do dia"** — um alvo `make check` que monta boot +
  init + tswl + swl-ui e roda os testes existentes, pra qualquer IA
  rodar antes de dizer "testei e funcionou" (evita o falso negativo que
  já custou uma sessão inteira).
- **Unit tests do parser ANSI do TSWL primeiro** — é o código mais
  exposto (entrada não-confiável) e o que tem mais bugs hoje. Testar
  antes de expandir o parser.

## GUI

- **Teste headless automatizado com `weston headless` / `WLR_BACKEND=headless`**:
  subir swlwm sem tela e validar que clientes mapeiam — mesmo tipo de
  checagem que pegou o bug do configure 0.18, sem depender de sessão
  gráfica do dev.
- **XWayland como decisão explícita** (DEC-003 deixou "ainda não
  decidido"). Vale uma nota no DECISIONS quando for discutido.
- **Barra de título em pt-BR alinhada ao tema** — detalhe de identidade,
  quando a GUI tiver mais apps nativos.
- **Fonte própria/pixel** para a identidade (README cita tipografia
  terminal/pixel) — começar com um tamanho/sombreado no theme, sem
  embutir font ainda.

## Plataforma

- **Padrão de "app nativo do catálogo"** — TSWL virou o padrão de
  arquitetura (wayland-client direto + cairo). Formalizar (ou não)
  num DEC o stack-base de apps, para as próximas 12 apps não divergirem.
- **Serviço de ícones centralizado** (README §10): o catálogo hoje é
  `default_icons[]` servindo desktop + menu; evoluir para uma API
  única quando houver 2+ apps de verdade.
- **Camada de software externo (README §14)** — só precisa de decisão
  quando houver um caso real (a ordem natural é: primeiro os apps
  nativos, depois a camada de compat).

## Processo

- **Formato de "PR de documento"**: quando uma IA quiser mudar uma
  decisão registrada (DECISIONS.md), ela abre a proposta no DIARIO em
  vez de editar direto — o admin valida e move.
- **Flag de sessão**: as sessões das IAs deveriam terminar sempre com
  "deixei X marcado no DIARIO para o orquestrador" — torna a orquestração
  reativa sem exigir os admins ficarem caçando.