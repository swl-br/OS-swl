### Documento de Trabalho: Pack de Assets Visuais (ícones + wallpapers)

**Status:** 🧪 EM DESENVOLVIMENTO
**Responsável Atual:** AI Visual Assets (Claude)
**IAs Relacionadas:** AI Integration Engine (Arquiteto, autor do swl-ui / DOC-WORK-001)

### 1. Escopo do Trabalho

Criar os pacotes de imagens do SWL OS: ícones de aplicativo (para substituir
os glifos vetoriais atualmente desenhados direto em `desktop.c`) e um
conjunto de wallpapers, seguindo a paleta e a estética "hacker retrô" já
definida em `swl-ui/include/theme.h`.

**Fora de escopo por enquanto:** qualquer alteração em `swlwm.c` /
`desktop.c` / `background.c` para de fato carregar esses arquivos no lugar
dos glifos e do grid procedural — isso fica para uma tarefa de código
separada, feita depois que os assets estiverem prontos e aprovados.

### 2. Decisões

- **Formato:** SVG puro (vetorial, escalável, cada arquivo só com alguns KB
  — o formato mais leve possível para ícones de linha e para wallpapers
  proceduralmente desenhados).
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
* [x] Criar os 13 ícones de app em SVG
* [x] Criar 3-5 wallpapers em SVG
* [ ] Integração em código (carregar os SVGs em vez dos glifos/grid) — tarefa futura, não incluída aqui

### 4. Registro de Alterações (Histórico)

* **03/09/2026 — Primeira entrega (Claude):** Criados `assets/icons/*.svg`
  (13 ícones) e `assets/wallpapers/*.svg` (conjunto inicial), todos
  seguindo a paleta de `theme.h`. Nenhum arquivo de código foi alterado
  nesta entrega.

### 5. Bugs, Bloqueios e Desafios Conhecidos

* Nenhum até o momento. Ponto de atenção para quem for integrar: o
  compositor hoje carrega PNG nativamente via Cairo sem lib extra; para
  usar os SVGs direto (sem pré-rasterizar) é necessário `librsvg` ou
  `resvg` como dependência nova do `swl-ui` — decisão de integração ainda
  não tomada.

### 6. Conclusão

* [ ] Aguardando aprovação do conjunto antes de fechar esta entrega.
