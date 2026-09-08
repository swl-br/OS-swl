# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: A5 — readaptar janelas maximizadas quando o output redimensiona

## Escopo

Apenas `swl-ui/src/swlwm.c`.

Não mexeu em: fullscreen (A6), apps, userland, B3.

Base: `origin/main` pós R-06 (`86a16ce`).

## Problema

Ao maximizar, a janela usava `screen_width` / `screen_height` atuais.
Se o output mudasse de modo depois (resize da janela X11/Wayland do
compositor, ou troca de resolução), só painel/taskbar/fundo eram
redimensionados em `output_request_state`. A janela maximizada ficava
no tamanho antigo (área morta ou cortada).

## Solução

1. Extraiu `toplevel_apply_maximized_layout()` — calcula área útil
   (tela − painel − taskbar − titlebar), posiciona em (0, PANEL),
   `set_size` + `set_maximized` + resize da decoração. **Não altera
   `saved_geo`** (restauração continua correta).

2. `toplevel_set_maximized(true)` passa a salvar geometria e chamar
   essa função (mesmo comportamento de antes, código compartilhado).

3. Em `output_request_state` (e no caminho de resize de `server_new_output`
   quando o shell já existe): após atualizar `screen_*` e o shell,
   itera `toplevels` e reaplica o layout em toda janela
   `maximized && !minimized`.

## Arquivo alterado

- `swl-ui/src/swlwm.c`

## Verificação

Leitura estática; sandbox sem wlroots. Teste sugerido no host:

```bash
cd swl-ui && meson setup build && ninja -C build
WLR_BACKEND=x11 ./build/swlwm -s "foot"   # ou tswl
# maximizar uma janela → redimensionar a janela do compositor
# a maximizada deve preencher a área útil de novo
# desmaximizar deve voltar ao saved_geo original
```

## Próximo passo sugerido

- Orquestrador: marcar A5 após teste interativo.
- A6 (fullscreen real) ou A7 (leveza de build) em seguida.
