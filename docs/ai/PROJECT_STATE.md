# PROJECT_STATE — SWL OS

Última atualização: 2026-09-04 (sessão com Claude, revisando trabalho do
Claude + OpenHands em paralelo)

## Kernel / Boot

STATUS: FUNCIONAL (ponta a ponta, testado em QEMU)

- Kernel Linux 7.2.1, configurado para x86 32-bit, config enxuta
  (`kernel/config/swl_defconfig`), sem drivers de hardware moderno
  desnecessário (USB 3.x, GPUs recentes, impressoras, etc).
- Bootloader 100% próprio, sem GRUB nem nenhum carregador externo:
  - `boot/boot.asm` (Estágio 1): lê disco via INT13h extendido (LBA),
    carrega o Estágio 2.
  - `boot/stage2.asm` (Estágio 2): lê um manifesto do disco (posição do
    kernel/initrd), carrega ambos em memória alta via INT15h AH=87h,
    monta os campos do cabeçalho de boot do Linux, e entrega o controle
    para o código de setup de 16-bit do próprio kernel.
- `userland/build-rootfs.sh` monta a raiz (bash estático + BusyBox via
  symlinks + `userland/init.asm` como `/init` e `/sbin/init`).
- `userland/build-disk.sh` monta a imagem de disco final (boot + stage2
  + manifesto + kernel + initrd).
- Testado com sucesso: boot completo até prompt de bash interativo.

Bugs conhecidos já resolvidos (não repetir): ver seção "Boot" do
`docs/decisoes.md` do repositório de trabalho anterior — principal:
o buffer de carregamento do kernel/initrd não pode ser reaproveitado
como área do código de setup do kernel (um sobrescreve o outro).

## Interface Gráfica (GUI)

STATUS: EM DESENVOLVIMENTO — base visual avançada, lógica de interação
parcialmente implementada.

Base adotada: `swl-ui/` — compositor Wayland baseado em wlroots 0.17.1,
partindo do `tinywl` (exemplo mínimo oficial) e evoluído para uma
arquitetura modular própria:

```
swl-ui/src/
├── swlwm.c              (núcleo do compositor, orquestra tudo)
├── background.c/h       (papel de parede)
├── panel.c/h            (painel superior: menu, CPU, MEM, relógio)
├── taskbar.c/h          (barra inferior: janelas abertas)
├── decorations.c/h      (barra de título das janelas, botões)
├── desktop.c/h          (ícones da área de trabalho + catálogo de apps)
├── menu.c/h             (menu iniciar — popup acima da taskbar)
├── swl_buffer.c/h       (integração cairo → wlr_scene_buffer)
└── swl_draw_util.c/h    (utilitários de desenho)
```

Compatibilidade wlroots: 0.17.1 (Xubuntu) e 0.18.2 (Mint) via guard de
compilação `SWL_WLR_0_18` em `swlwm.c` (ver DEC-006). As duas máquinas de
desenvolvimento usam versões diferentes — qualquer API nova do wlroots
precisa ser checada contra as duas antes de assumir que compila nas duas
máquinas.

Build: `meson setup build && ninja -C build`
Teste (dentro de sessão gráfica X11 existente, sem sair do host):
`WLR_BACKEND=x11 ./build/swlwm -s "foot"`
(cliente de teste precisa ser Wayland nativo — `xterm` NÃO funciona,
só engana visualmente por rodar fora do nosso compositor. Desde
2026-09-04 já existe um cliente nativo do próprio projeto pra testar:
`../apps/tswl/build/tswl`, ver seção "Aplicativos" abaixo)

### O que já funciona (testado e confirmado)
- Fundo, painel superior (com CPU/MEM reais, relógio), taskbar, ícones
  visuais na área de trabalho — tudo desenhado corretamente.
- Redimensionar a janela de teste: painel/taskbar/fundo se recalculam
  certo.
- Arrastar janela pela barra de título própria: funcional (inclusive
  desmaximiza automaticamente se a janela estava maximizada).
- Fechar janela pelo botão da decoração: funcional.
- **Maximizar/minimizar** (2026-09-03, Claude): funcional, validado
  interativamente pelo usuário no Xubuntu — maximizar, minimizar,
  arrastar (com desmaximizar automático), fechar e restaurar
  clicando na janela minimizada na taskbar, todos confirmados.
- **Menu iniciar** (2026-09-04, Claude + OpenHands): funcional no
  código (compilado e testado headless nas duas versões de wlroots,
  0.17.1 e 0.18.2 — ver sessões de 2026-09-04). Reaproveita o catálogo
  de apps de `desktop.c` (`swl_desktop_app_count/label/command()`) —
  mesma fonte pros ícones do desktop e pro menu. **Ainda não validado
  interativamente** (ver "Não está implementado ainda" abaixo — o
  primeiro patch chegou incompleto ao repositório, foi corrigido pela
  OpenHands, e a validação visual do usuário até agora só rodou contra
  o estado incompleto).

### Bugs corrigidos (histórico)
1. **2026-09-03 (Claude)**: redimensionamento não propagava pro
   painel/taskbar/fundo (`output_request_state()` não chamava os
   resizes). Corrigido.
2. **2026-09-03 (Claude)**: crash ao arrastar janela pela barra de
   título própria quando `focused_surface == NULL` (clique vindo da
   nossa decoração, não de uma superfície de cliente real). Corrigido.
3. **2026-09-04 (OpenHands)**: no wlroots 0.18, nenhum cliente
   conseguia mapear janela — faltava agendar o configure inicial no
   primeiro commit (`xdg_toplevel_commit`), e o xdg-shell mudou de um
   único evento `new_surface` pra dois (`new_toplevel`/`new_popup`)
   disparando antes da surface estar inicializada. Corrigido atrás do
   guard `SWL_WLR_0_18` — ver DEC-006. Bug crítico: sem ele, TODO
   cliente Wayland travava ao abrir no Mint (0.18), incluindo o
   próprio `foot` usado nos testes anteriores.

### O que NÃO está implementado ainda (não fingir que está pronto)
- **Menu iniciar: falta validação interativa real** (clicar no botão
  MENU e ver a lista aparecer/desaparecer, clicar num item, clicar
  fora fechando, clicar numa janela atrás focando). O código compila e
  roda headless sem crash nas duas versões de wlroots, mas ninguém
  ainda confirmou visualmente que aparece na tela — a única tentativa
  de validação (2026-09-04) rodou contra uma cópia do repo onde o
  patch do menu só tinha chegado pela metade (só `menu.c`/`menu.h`,
  sem `meson.build`/`swlwm.c` atualizados), então não é uma validação
  válida do estado atual.
- Trocar papel de parede: não implementado.
- Arrastar ícones da área de trabalho, adicionar/remover atalhos: não
  implementado (ícones são fixos, não-interativos além do clique que
  já abre programas).
- Lista de janelas abertas na taskbar: mostra título e foca ao
  clicar, mas sem preview/thumbnail.
- Janela maximizada não readapta o tamanho se o output for
  redimensionado depois (ver sessão de maximizar/minimizar).
- Fullscreen real: só responde ao protocolo, sem lógica de verdade.
- Não roda ainda no hardware real / initramfs do SWL OS — só testado
  na sessão gráfica X11 do desenvolvedor (`WLR_BACKEND=x11`).
  Integração com o boot real (rodar como o processo gráfico principal
  dentro do initramfs, sem X11 por baixo, via DRM/KMS) ainda não foi
  feita — é a próxima tarefa grande da GUI.

## Linguagem SWL / Compilador swlc

STATUS: NÃO INICIADO.

## Aplicativos

STATUS: EM DESENVOLVIMENTO — primeiro app nativo criado.

- **TSWL** (`apps/tswl/`, 2026-09-04, OpenHands): terminal nativo, sem
  toolkit (wayland-client + xdg-shell + cairo/pango direto), parser
  ANSI/VT100 próprio, PTY via `forkpty`. Binário ~57 KB. Build
  autônomo (`cd apps/tswl && meson setup build && ninja -C build`).
  Compilado e testado (headless, sem crash, sem busy-loop) contra
  wlroots 0.17.1 — ver sessão de 2026-09-04 (Claude). Ainda falta
  validação interativa de teclado de verdade (digitar, scroll,
  cores) numa sessão gráfica real.
- Os outros 12 itens do catálogo (`default_icons[]` em `desktop.c`:
  SWLPad, gerenciador de arquivos, SEBRE, Configurações, etc.)
  continuam sendo só placeholders — comando aponta pra um binário que
  não existe ainda.

## Próximos passos sugeridos (GUI)
1. Validar interativamente o menu iniciar (código pronto, nunca
   validado contra o estado completo — ver ressalva acima).
2. Validar interativamente o TSWL (`WLR_BACKEND=x11 ./build/swlwm -s
   "../apps/tswl/build/tswl"`) — digitar, cores, scroll, resize.
3. Integrar o compositor ao boot real (substituir o teste
   `WLR_BACKEND=x11` por rodar como sessão gráfica principal via
   DRM/KMS, dentro do ambiente do SWL OS de verdade).
4. Readaptar janelas maximizadas quando o output redimensiona.
5. Fullscreen real (protocolo já responde, falta lógica).
