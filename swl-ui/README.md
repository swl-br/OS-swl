# SWL UI — shell gráfico do SWL OS

Isto estende o `swlwm` (baseado em tinywl/wlroots) que vocês já tinham,
adicionando a interface visual: painel superior, taskbar, decoração de
janelas, ícones de área de trabalho e wallpaper — tudo no estilo "hacker
retrô" do mockup.

## Estrutura

```
include/
  theme.h          paleta de cores, fonte, métricas (SWL_PANEL_HEIGHT etc.)
  swl_buffer.h      ponte genérica Cairo -> wlr_scene_buffer
  swl_draw_util.h   texto (Pango) e formas básicas
  panel.h           barra superior (CPU/MEM reais via /proc, relógio, menu)
  taskbar.h         barra inferior (MENU, janelas abertas, bandeja, relógio)
  decorations.h     barra de título por janela (fechar/maximizar/minimizar)
  desktop.h         grade de ícones da área de trabalho (glifos vetoriais)
  background.h      wallpaper (PNG com "cover" ou grade procedural)
src/
  swlwm.c           o compositor, com os pontos de integração já aplicados
  (+ .c de cada header acima)
meson.build
```

## Como a decoração de janela funciona

Cada `tinywl_toplevel` agora tem **duas** scene trees em vez de uma:

- `scene_tree` — o "wrapper". Mover ou levantar este nó move a janela
  inteira (título + conteúdo) de uma vez. É nele que a barra de título é
  criada, na posição local (0,0).
- `content_tree` — a subárvore da superfície xdg de fato (o que o
  `wlr_scene_xdg_surface_create` original criava), deslocada
  `SWL_TITLEBAR_HEIGHT` pixels para baixo dentro do wrapper.

Isso significa que mover/focar a janela continua sendo uma única chamada
(`wlr_scene_node_set_position` / `wlr_scene_node_raise_to_top` no
`scene_tree`), e a barra de título "gruda" na janela automaticamente.

O preço disso: os cálculos de resize (`begin_interactive` e
`process_cursor_resize`) precisaram de um offset de `SWL_TITLEBAR_HEIGHT`
pixels — já ajustado, comentado no código nos dois lugares.

## Build

```sh
cd swl-ui
meson setup build
ninja -C build
./build/swlwm            # roda aninhado, se você já tiver um Wayland/X11 rodando
# ou, no TTY real:
./build/swlwm -s "seu-app-de-teste"
```

Compilação verificada de ponta a ponta (meson + ninja + link). O
`meson.build` procura wlroots nessa ordem: `wlroots-0.19`,
`wlroots-0.18`, e o genérico `wlroots` (o pacote que o pkg-config da
máquina tiver.. Nas máquinas atuais (Xubuntu/Mint) isso cai em
wlroots 0.17.1 ou 0.18.2, e o código tem o guard de
compilação `SWL_WLR_0_18` para funcionar com ambas (ver
`docs/ai/DECISIONS.md`, DEC-006..

Dependências: `wlroots`, `wayland-server`, `xkbcommon`, `cairo`,
`pangocairo`, `libdrm`.

## O que foi corrigido

- **`#error "Add -DWLR_USE_UNSTABLE..."` em quase todo `.c`**: os headers
  do wlroots exigem essa macro pra liberar a API atual (a "0.19 estável"
  ainda não existe do lado deles). Faltava passar `-DWLR_USE_UNSTABLE` pro
  compilador — adicionado em `c_args` no `meson.build`.
- **`M_PI` e `strdup` "undeclared" em `desktop.c`, `decorations.c`,
  `swlwm.c`, `taskbar.c`**: com `c_std=c11` puro (sem extensões GNU), a
  glibc esconde essas duas atrás de feature-test macros. Adicionado
  `-D_DEFAULT_SOURCE` junto no `meson.build` (não precisa trocar pra
  `gnu11` nem incluir headers extras).
- **Bug silencioso em `theme.h`**: as cores eram listas soltas de 4
  `double` separados por vírgula, coladas dentro dos parênteses de
  `swl_set_color()` pela macro `SWL_SET`. Isso funciona para uma cor fixa,
  mas quebra dentro de um ternário — ex.
  `SWL_SET(cr, focused ? SWL_COL_ACCENT_CYAN : SWL_COL_TEXT_DIM)` — porque
  a gramática do C permite o operador vírgula no ramo do meio do ternário,
  então o resultado final não é "escolhe uma cor ou outra", é "descarta
  quase todos os canais e sobra um valor solto", só avisado por um warning
  de "comma expression has no effect" (o `taskbar.c` e `decorations.c` do
  logs de build tinham exatamente esse warning nos ternários de foco). Troquei
  a paleta pra um `typedef struct { double r,g,b,a; } swl_color_t;` com
  compound literals (`SWL_COLOR(r,g,b,a)`), aí o ternário compara dois
  valores do mesmo tipo normalmente e o compilador para de reclamar — e o
  resultado passa a ser a cor certa de verdade, não só "sem warning".

## Sobre o wallpaper

`swl_background_create()` aceita um caminho de PNG opcional; o Cairo
carrega PNG nativamente, sem precisar de libpng/librsvg extra. Isso já
existia, mas nunca era usado — as duas chamadas em `swlwm.c` passavam
`NULL` fixo, então o compositor sempre caía no grid procedural, nunca no
wallpaper de verdade.

Agora:

- `./build/swlwm -b caminho/para/wallpaper.png` define o wallpaper na hora.
- Sem `-b`, o `main()` procura, nessa ordem, `assets/wallpaper.png`,
  `../assets/wallpaper.png`, `/usr/local/share/swl-ui/wallpaper.png` e
  `/usr/share/swl-ui/wallpaper.png`; usa o primeiro que existir e só cai no
  grid procedural se nenhum for encontrado.
- A arte padrão já está em
  `assets/wallpaper.png` (com as variações-fonte em SVG em `assets/wallpapers/`).
  Rodando `./build/swlwm` a partir da raiz do repo, ela é achada
  automaticamente, sem precisar editar código nem hardcodar um path
  absoluto de sistema.

## Funcionalidades implementadas

- **Painel superior** (CPU/MEM reais via `/proc`, relógio)e **taskbar**
  (janelas abertas, bandeja) funcionais..
- **Menu iniciar**: abre/fecha pelo botão MENU,e clique em item lança o
  app (catálogo vindo de `desktop.c`).
- **Maximizar/minimizar/arrastar/fechar**: implementados e validados
  interativamente.

- **Wallpaper** (`assets/wallpaper.png` ou `-b <caminho>`).
- **Ícones de app**: glifos vetoriais (leves, sem I/O).

## O que ainda falta (TODO real)

- **Trocar papel de parede em runtime**: não implementado (o wallpaper é fixo
  no boot do compositor`.
- **Ícones da área de trabalho interativos**: hoje abrem apps no clique, mas
  arrastar/adicionar/remover atalhos não..
- **Preview/thumbnail** de janelas na taskbar: lista mostra título e foca ao
  clicar, mas sem preview visual..

- **Fullscreen real**: o protocolo responde, mas falta a lógica de verdade..
- **Multi-monitor**: o código assume um layout único cobrindo a resolução do
  primeiro output. Generalizar exige guardar painel/taskbar por output.
 
## Paleta (`theme.h`)

Se quiser ajustar as cores pra bater ainda mais com a arte de referência, é
só mexer em `include/theme.h` — todo o resto do código usa `SWL_SET(cr,
SWL_COL_X)` em vez de valores soltos, então muda tudo de uma vez.
