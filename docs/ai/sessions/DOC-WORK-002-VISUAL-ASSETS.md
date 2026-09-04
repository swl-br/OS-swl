### Documento de Trabalho: Pack de Assets Visuais (ícones + wallpapers)

**Status:** 🧪 EM DESENVOLVIMENTO
**Responsável Atual:** AI Visual Assets (Claude)
**IAs Relacionadas:** AI Integration Engine (Arquiteto, autor do swl-ui / DOC-WORK-001)

### 1. Escopo do Trabalho

Criar os pacotes de imagens do SWL OS: ícones de aplicativo (para substituir
os glifos vetoriais atualmente desenhados direto em `desktop.c`) e um
conjunto de wallpapers, seguindo a paleta e a estética "hacker retrô" já
definida em `swl-ui/include/theme.h`.

**Atualização:** a integração em código, inicialmente adiada, foi puxada
pra esta mesma entrega a pedido — ver seção 3 e 4.

### 2. Decisões

- **Formato dos ícones (fonte):** SVG (vetorial, ~200-600 bytes cada).
- **Formato dos ícones (runtime):** rasterizados para PNG 80x80 (2x o
  tamanho exibido, 40px). Motivo: o compositor já carrega PNG nativo via
  Cairo (`cairo_image_surface_create_from_png`), sem dependência nova.
  Carregar SVG direto exigiria `librsvg`/`resvg` como dependência nova do
  `swl-ui` — adiado até termos motivo real pra pagar esse custo (ex.:
  precisar de tamanhos arbitrários sem gerar arquivo por resolução). O SVG
  continua sendo a fonte de verdade; o PNG é gerado a partir dele.
- **Wallpaper:** `waypaper` inicial (`grid.svg`) também rasterizado pra
  PNG 1920x1080 (~123KB) e colocado em `swl-ui/assets/wallpaper.png` — é
  exatamente o primeiro caminho que `swlwm.c` já procura sozinho, então
  não precisou mexer no `main()`.
- **Paleta:** reaproveitada 1:1 de `theme.h` (fundo `#0b0e14`, painel
  `#0e1219`, ciano `#6bd1cc`, roxo `#bf9edb`, dourado `#d4a55c`, texto
  `#d4dee6`/`#737f8f`, vermelho de perigo `#dc5a66`), para não criar uma
  segunda fonte de verdade de cor.
- **Estilo dos ícones:** linha monocromática (stroke), cantos arredondados,
  fundo transparente — continuação direta do estilo já usado nos glifos de
  `desktop.c`, só que como arquivo em vez de código inline.
- **Conjunto de ícones:** os 13 já referenciados em `desktop.c`
  (`default_icons[]`): TSWL, SWLPAD, ARQUIVOS, SEBRE, CONFIG, DRIVERS, REDE,
  SISTEMA, MEDIAPLAYER, IMAGENS, JOGOS, COMPACTAR, LIXEIRA.
- **Wallpapers:** 3 a 5 variações, todas SVG (padrões procedurais/gradiente
  — nada de fotografia, para manter leveza e consistência com o fallback
  procedural que já existe em `background.c`).

### 3. O que está sendo feito / Planejamento

* [x] Definir paleta e estilo (reaproveitado de `theme.h`)
* [x] Criar os 13 ícones de app em SVG (fonte) + rasterizar em PNG (runtime)
* [x] Criar 3-5 wallpapers em SVG; rasterizar o primeiro (`grid`) em PNG
* [x] Integrar os PNGs em `desktop.c` (ícones do desktop) com fallback pro glifo vetorial
* [x] Integrar o wallpaper PNG (caminho já era procurado por `swlwm.c`, nenhuma mudança de código necessária)
* [x] Validar build (meson+ninja) e execução headless sem crash
* [ ] Estilo ainda não é o definitivo do sistema — pack completo fica pra uma próxima rodada

### 4. Registro de Alterações (Histórico)

* **04/09/2026 — Redesign pixel-art a partir da referência oficial (Claude):**
  Vocês mandaram a imagem oficial da GUI (feita por GPT) — o estilo real é
  **pixel-art/blocado com sombreamento**, bem diferente da minha primeira
  leva (linha fina/vetorial). Refiz os 13 ícones do zero: formas
  geométricas simples (retângulos/polígonos) num grid de baixa resolução
  (32x32), rasterizadas sem suavização e ampliadas com nearest-neighbor
  pra manter a borda "pixelada" de propósito. Trocas de iconografia pra
  bater com a referência: REDE virou antena/wifi (antes era um grafo de
  nós, não combinava); LIXEIRA virou ciano/teal (a referência não usa
  vermelho ali). **Duas cores novas que a referência usa e que não
  existem em `theme.h` ainda:** verde (`drivers`/chip, ~#5fbf7a) e
  magenta/rosa (`jogos`, ~#d868a8) — usei valores aproximados nos ícones,
  mas fica pendente decidir se essas cores entram oficialmente em
  `theme.h` ou se ficam só nos ícones. SVG (fonte) e PNG (runtime, ver
  decisão da seção 2) atualizados juntos. Nenhum `.c` mudou nesta etapa.
  **Limitação importante:** o wallpaper "gato pixelado hacker-retrô" da
  referência é uma ilustração bem mais elaborada (arte detalhada, não só
  formas geométricas) — reproduzir ela fielmente está fora do que dá pra
  fazer só com desenho vetorial/código; os 5 wallpapers já entregues
  (grid/circuit/skyline/terminal-glow/waves) continuam sendo composições
  proceduais simples na mesma paleta, não uma cópia dessa arte
  específica.
* **03/09/2026 — Correção de layout no painel (Claude):** No teste de
  vocês, o bloco direito do painel (`panel.c`) mostrou "MEM ..." e a
  data/hora sobrepostos e ilegíveis. Causa: o layout usava um offset
  fixo (`width - 300`) que não considerava a largura real dos textos —
  CPU label + barra + MEM label + data/hora somados passam bem de 300px,
  então em qualquer janela abaixo de um certo tamanho um desenhava por
  cima do outro. Corrigido: agora cada texto é medido com
  `swl_text_extents()` e o bloco é montado da direita pra esquerda antes
  de desenhar qualquer coisa; se não couber tudo sem invadir o menu, o
  bloco inteiro some (nunca mais sobrepõe texto). Build e execução
  headless testados de novo, sem crash.
* **03/09/2026 — Primeira entrega, só assets (Claude):** Criados
  `assets/icons/*.svg` (13 ícones) e `assets/wallpapers/*.svg` (5
  variações), todos seguindo a paleta de `theme.h`. Nenhum arquivo de
  código alterado nesta etapa.
* **03/09/2026 — Integração em código (Claude):** Rasterizados os 13
  ícones para PNG 80x80 em `swl-ui/assets/icons/*.png` e o wallpaper
  `grid.svg` para `swl-ui/assets/wallpaper.png` (1920x1080). Modificado
  `swl-ui/src/desktop.c`: struct `swl_icon_def` ganhou campo `icon_file`;
  nova função `resolve_icon_path()` (mesmo padrão de busca de caminho que
  `swlwm.c` já usa pro wallpaper: `assets/icons/`, `../assets/icons/`,
  `/usr/local/share/swl-ui/icons/`, `/usr/share/swl-ui/icons/`);
  `icon_cell_draw()` agora tenta carregar o PNG primeiro e só cai no
  glifo vetorial antigo se o arquivo não for encontrado ou falhar ao
  carregar — nenhum ícone quebra por falta de asset. Build limpo
  (`meson setup build && ninja -C build`) e execução headless
  (`WLR_BACKENDS=headless WLR_RENDERER=pixman`) testados sem crash.

### 5. Bugs, Bloqueios e Desafios Conhecidos

* Nenhum. `wlroots` instalado no ambiente de teste foi a 0.17 (não a
  0.19 que o `meson.build` prefere) — o próprio `meson.build` já cai pro
  fallback genérico `wlroots`, compilou e rodou normal.
* Estilo dos assets ainda não é o definitivo — combinado que o pack
  completo/final vem depois, essa leva é só pra destravar o teste.

### 6. Conclusão

* [x] Assets criados, integrados em código e testados sem crash.
* [ ] Aguardando teste visual de vocês (rodando de verdade, não headless) antes de fechar esta entrega.
