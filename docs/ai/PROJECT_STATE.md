# PROJECT_STATE — SWL OS

Última atualização: 2026-09-05 (orquestrador — ver docs/orquestracao/;
reorganização da doc principal pela OpenHands; ver DEC-008)

## Repositório (higiene)

- Clone íntegro: 95.222 arquivos, `git status` limpo.
- `.gitignore` subido em 2026-09-05 com o nome errado (`gitignore`, sem
  ponto) — **corrigido para `.gitignore` pelo orquestrador** na mesma
  data. O arquivo já cobre `/build/`, `*.img`, `*.cpio.gz`, `*.bin`,
  `*.log`, `/rootfs/` e `/kernel/linux-7.2.1/` para arquivos NOVOS.
- **Rastreado ainda precisa de remoção manual** (`git rm --cached`):
  `build/*`, `rootfs/*`, `kernel/linux-7.2.1/` (94.856 arquivos) e
  qualquer artefato já commitado (tarefa A3 em
  `docs/orquestracao/AFAZERES.md`; achado R-08 em `docs/revisao/`).
- Dependências externas agora têm script: `userland/fetch-deps.sh`
  (kernel + bash/busybox estáticos).

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

Bug conhecido já resolvido (não repetir): o buffer de carregamento do kernel/initrd
não pode ser reaproveitado como área do código de setup do kernel (um sobrescreve o outro).

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
- **Menu iniciar** (2026-09-04, Claude + OpenHands): **funcional,
  validado interativamente pelo usuário na máquina Mint** — abre/fecha
  pelo botão MENU, clique em item lança o app (placeholder), clique
  fora fecha, catálogo reaproveitado de `desktop.c`
  (`swl_desktop_app_count/label/command()`).

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
  **Funcional, validado interativamente pelo usuário na máquina
  Mint** — abre, roda o shell, digita normalmente.
- Os outros 12 itens do catálogo (`default_icons[]` em `desktop.c`:
  SWLPad, gerenciador de arquivos, SEBRE, Configurações, etc.)
  continuam sendo só placeholders — comando aponta pra um binário que
  não existe ainda.

## Revisões / QA

- Revisão de código estática periódica feita pelos orquestradores.
  Achados ficam em `docs/revisao/` com responsáveis e status.
  Ver `docs/revisao/2026-09-05-revisao-01.md` (R-01 a R-14).
  Críticos atuais: corrupção de memória e null deref no parser/startup
  do TSWL (R-01 a R-03), abertos — pedido de correção à OpenHands.

## Espaço de orquestração

- `docs/orquestracao/` — lista-mestra de tarefas desbloqueadas
  (`AFAZERES.md`), ideias (`IDEIAS.md`), diário (`DIARIO.md`) e regras.
  Área exclusiva dos orquestradores (admin + GPT); implementação não edita.

## Próximos passos sugeridos (GUI)
1. Integrar o compositor ao boot real (substituir o teste
   `WLR_BACKEND=x11` por rodar como sessão gráfica principal via
   DRM/KMS, dentro do ambiente do SWL OS de verdade).
2. Readaptar janelas maximizadas quando o output redimensiona.
3. Fullscreen real (protocolo já responde, falta lógica).
