# SWL UI — shell gráfico do SWL OS

Isto estende o `swlwm` (baseado em tinywl/wlroots) que vocês já tinham,
adicionando a interface visual: painel superior, taskbar, decoração de
janelas, ícones de área de trabalho e wallpaper — tudo no estilo "hacker
retrô" do mockup.

## O que mudou em relação ao que vocês tinham

- `icons.c`, `clock.c` e `text_label.c` foram **substituídos**. A técnica de
  base (Cairo desenhando num buffer ARGB8888 empacotado como
  `wlr_scene_buffer`) era boa e foi mantida — só generalizada em
  `swl_buffer.c`, que qualquer widget novo pode reaproveitar.
- A diferença de desempenho importante: o `clock.c` antigo **destruía e
  recriava o nó da scene graph a cada segundo**. Agora `swl_buffer_redraw()`
  troca só o pixel buffer por trás de um nó que já existe
  (`wlr_scene_buffer_set_buffer`), bem mais barato.
- A decoração de janela (fechar/mover) que existia era só uma **hitbox
  invisível** com coordenadas fixas (`ty - 22`, `20x20`...). Agora existe
  uma barra de título de verdade, desenhada, que sabe sua própria largura —
  o hit-test usa essas mesmas coordenadas em vez de números soltos.

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

Compilação verificada de ponta a ponta (meson + ninja + link) contra
wlroots 0.17 genérico (o mesmo fallback que o `meson.build` escolhe quando
não acha o `wlroots-0.19` via pkg-config) e testada de pé com
`WLR_BACKENDS=headless`, sem crash. Os três erros que travavam o build
eram bugs reais no código, não falta de dependência — ver "O que foi
corrigido" abaixo.

Dependências: `wlroots` (0.19, ou ajuste o meson.build pra sua versão),
`wayland-server`, `xkbcommon`, `cairo`, `pangocairo`, `libdrm`.

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
  log de vocês têm exatamente esse warning nos ternários de foco). Troquei
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
- A arte do gato pixelado que vocês mandaram já está em
  `assets/wallpaper.png` — rodando `./build/swlwm` a partir da raiz do
  repo (`~/Downloads/swl-ui`, como no log de vocês) ela é achada
  automaticamente, sem precisar editar código nem hardcodar um path
  absoluto de sistema.

## O que ficou como TODO (de propósito, pra não inflar demais esta entrega)

- **Menu iniciar**: o clique no botão MENU da taskbar já está capturado
  (`hit == -1` em `server_cursor_button`), só falta desenhar o menu em si.
- **Maximizar/minimizar**: os botões já existem visualmente e o clique já é
  capturado; falta guardar geometria original e implementar o toggle
  (maximizar) e o estado "escondido, só na taskbar" (minimizar).
- **Ícones de app com imagem própria**: hoje são glifos vetoriais (leves,
  sem I/O). Se quiserem ícones customizados por app depois, dá pra estender
  `swl_desktop_icon_def` com um caminho de PNG opcional, usando a mesma
  técnica de `background.c`.
- Multi-monitor: o código assume um layout único cobrindo a resolução do
  primeiro output. Funciona para o caso de vocês agora; generalizar exige
  guardar painel/taskbar por output.

## Paleta (`theme.h`)

Se quiser ajustar as cores pra bater ainda mais com a arte de referência, é
só mexer em `include/theme.h` — todo o resto do código usa `SWL_SET(cr,
SWL_COL_X)` em vez de valores soltos, então muda tudo de uma vez.
