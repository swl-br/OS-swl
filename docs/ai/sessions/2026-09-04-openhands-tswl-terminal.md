# Sessão — 2026-09-04 (4)

IA: OpenHands
Data: 2026-09-04
Responsável: criar o TSWL (primeiro app nativo — terminal) + corrigir
bug crítico do compositor que impedia QUALQUER cliente de mapear no
wlroots 0.18
Branch: main

## Objetivo

Criar o primeiro aplicativo nativo do SWL OS: o terminal TSWL, seguindo
os princípios do projeto — apenas C/Assembly, leveza, identidade própria,
sem toolkits pesados. No meio do caminho, descobri e corrigi um bug no
compositor (swlwm) que impedia foot/tswl/qualquer cliente de receber o
configure inicial no wlroots 0.18.

## O que foi criado

`apps/tswl/` — primeiro app nativo, estrutura própria:

```
apps/tswl/
├── meson.build          build autônomo (gera xdg-shell client via
│                        wayland-scanner a partir do XML do sistema)
├── include/
│   ├── term.h           API do modelo de terminal
│   ├── render.h         API do renderer cairo
│   └── tswl_pty.h       API do pseudo-terminal
└── src/
    ├── main.c           loop Wayland + PTY + teclado (um poll, sem threads)
    ├── term.c           grid de células + parser ANSI/VT100
    ├── render.c         desenho cairo/pango com a paleta do theme.h
    └── pty.c            forkpty + resize do PTY
```

Arquivos modificados:
- `swl-ui/src/swlwm.c` — bug do configure inicial no wlroots 0.18
  (ver "Bug crítico encontrado" abaixo).
- `swl-ui/meson.build`, `swl-ui/src/desktop.c`, `swl-ui/include/desktop.h`
  — já tinham sido entregues na sessão anterior (menu iniciar); incluídos
  no pacote desta sessão porque ainda não estavam no repo remoto.

## Arquitetura do TSWL

Princípios aplicados (leveza, sem bloat):

- **Um único loop de eventos** via `poll()` no fd do Wayland + fd mestre
  do PTY — sem threads, sem timers separados além do blink do cursor.
- **Grid de células contíguo** (`cols*rows`, ~12 bytes/célula): texto +
  fg/bg/attrs compactados em bits. Scrollback em ring buffer de 1000
  linhas pré-alocado. Nenhuma alocação acontece por byte processado.
- **Parser ANSI/VT100 como máquina de estados** byte a byte, com
  decodificação UTF-8 incremental embutida. Cobre: cores 8/bright,
  negrito/dim/underline/reverse, movimentação de cursor, erase
  (linha/tela/chars), scroll region, insert/delete lines/chars,
  save/restore cursor, alt screen (básico), OSC ignorado com segurança.
- **Renderização sob demanda**: só redesenha quando o terminal marca
  mudança ou o cursor pisca. Fundo pintado em segmentos de mesma cor
  (não célula a célula); texto em runs de mesmo estilo com um único
  PangoLayout reutilizado.
- **Sem toolkit**: wayland-client + xdg-shell + shm buffers ARGB8888 +
  cairo/pango. Dependências: wayland-client, xkbcommon, cairo,
  pangocairo. Binário final: **~57 KB**.

## Bug crítico encontrado (no compositor, não no TSWL)

**Sintoma**: qualquer cliente (foot, tswl) conectava no swlwm mas nunca
mapeava a janela — ficava travado esperando o configure inicial.

**Diagnóstico**: no wlroots 0.18, `new_toplevel` dispara antes da
surface estar inicializada, e o configure inicial NÃO é enviado
automaticamente no commit (como era no 0.17). O compositor precisa
chamar `wlr_xdg_surface_schedule_configure()` — mas no momento certo:
cedo demais (new_toplevel) dá "configure scheduled for uninitialized
xdg_surface"; em todo commit cria loop infinito (configure → commit →
schedule → configure... 59 mil configures em 6s num teste).

**Correção**: em `xdg_toplevel_commit()` (caminho 0.18), agenda UMA vez
no primeiro commit do cliente (flag `initial_configure_sent` no
toplevel). Isso destrava o map de qualquer app.

**Importante**: esse bug existia desde que o compositor passou a rodar
no wlroots 0.18 — o foot "parecia funcionar" em testes anteriores
(só checávamos "conectou/criou surface") mas também estava travado.
A validação do menu na máquina do usuário (0.17) não pegou isso porque
o caminho 0.17 não precisa do schedule.

## Testes

Ambiente: container Debian 13, wlroots 0.18.2, sem sessão gráfica
interativa.

- ✅ `meson setup build && ninja -C build` (tswl): compila limpo, zero
  warnings.
- ✅ Teste unitário do parser+render (probe): texto, cores ANSI
  (fg/bold/bright), atributos verificados célula a célula.
- ✅ Teste E2E real: spawn do bash num PTY → comando `echo TSWL-E2E-OK`
  → saída apareceu na linha correta do grid. Fluxo PTY → parser → grid
  → render validado de ponta a ponta.
- ✅ TSWL dentro do swlwm (headless): configure chega, 2 acks, 13 buffer
  attaches (janela desenhando), sem erro no compositor.
- ✅ Foot dentro do swlwm após a correção: 10 surfaces, zero erros
  (confirma que a correção beneficia todos os clientes).
- ❌ NÃO testado: interação real de teclado (digitar no terminal com a
  GUI visível) — precisa do teste interativo no Mint/Xubuntu.

## Como testar interativamente

```bash
# 1) extrair o pacote TSWL-e-compositor.tar.gz por cima do repo
# 2) rebuild do compositor (tem a correção do configure):
cd swl-ui && rm -rf build && meson setup build && ninja -C build && cd ..
# 3) build do terminal:
cd apps/tswl && meson setup build && ninja -C build && cd ../..
# 4) rodar a GUI com o terminal:
cd swl-ui && WLR_BACKEND=x11 ./build/swlwm -s ../apps/tswl/build/tswl
```

Testar: janela TSWL abre com prompt do shell; digitar comandos; cores
(`ls --color`); setas/Home/End/Delete; PageUp/Down; Ctrl+C; Shift+PageUp
(scrollback); redimensionar a janela (grid reajusta); fechar.

## Problemas conhecidos (TSWL v0.1)

- Seleção de texto / clipboard: não implementado.
- Scrollback: 1000 linhas fixas; resize zera o histórico.
- Alt screen (htop/less): entra/sai limpando a tela, sem restaurar o
  conteúdo anterior.
- Fonte fixa (JetBrains Mono/Fira Code/monospace, 12pt): sem config.
- Menu iniciar/ícone TSWL do desktop apontam o comando `tswl` — pra
  abrir por lá, o binário precisa estar no PATH (ou o comando ajustado
  pro caminho completo). Decisão de instalação do app fica pro projeto.

## TODO

1. Validação interativa do TSWL no Mint/Xubuntu (usuário).
2. Subir o código pro repo (usuário faz upload pelo site).
3. Próximas iterações do TSWL: clipboard, seleção, cores 256, ligar no
   catálogo de apps com instalação de verdade.

## Integração

Qualquer IA que mexer nisso deve saber:
- `swlwm.c` (caminho 0.18): NÃO remover o `schedule_configure` do
  primeiro commit em `xdg_toplevel_commit` — sem ele nenhum cliente
  mapeia. E não agendar em todo commit (loop infinito) — a flag
  `initial_configure_sent` existe por isso.
- O header `apps/tswl/include/tswl_pty.h` não pode se chamar `pty.h`:
  ele fazia shadowing do `<pty.h>` do sistema e o forkpty sumia.
- O protocolo xdg-shell do app é gerado em modo CLIENTE pelo
  wayland-scanner no build (diferente do swl-ui, que usa o arquivo
  server-side commitado).
