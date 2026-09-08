# PROJECT_STATE — SWL OS

Última atualização: 2026-09-07 (orquestrador — boot GUI real funcional,
recuperação da pasta, push no `main`; ver
`docs/ai/sessions/2026-09-07-recuperacao-pasta-boot-gui.md`)

## Repositório (higiene)

- Histórico recomeçado em 2026-09-07 (pasta oficial apagada; ver sessão
  acima): `main` atual tem só fontes + assets (969 arquivos no commit
  base), `git status` limpo. Histórico antigo (54 commits, incluía a
  árvore do kernel ~1,8 GB) preservado na tag `main-arquivo`.
- `.gitignore` cobre `/build/`, `*.img`, `*.cpio.gz`, `*.bin`, `*.log`,
  `/rootfs/`, `/gui-artifacts/`, `/kernel/*` (exceto
  `kernel/config/swl_defconfig`, que o `fetch-deps.sh` exige),
  `apps/*/build/`, `swl-ui/build/`. Tarefa A3 (git rm de artefatos)
  está SUPERADA — nada rastreado além de fontes.
- `userland/build-initramfs.sh` (R-16) recuperado do `main` antigo;
  `Makefile` tem regra de initramfs e o disco depende dela (acaba com
  o disco desatualizado parecia-atual).

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

Bugs de boot resolvidos em 2026-09-07 (não repetir):
- stage2 carregava o initrd colado no kernel (~6,5 MB); o kernel se
  autodescomprime por cima (~20–30 MB) e initrds >13 MB morriam com
  `Initramfs unpacking failed`. Fix: `INITRD_TOP 0x17000000`.
- `/bin/sh` era bash 64-bit do host (inexequível no i386) → `sh` agora
  aponta pro busybox i386 (idem no `build-rootfs.sh`).
- init monta `devpts` (`/dev/pts`, gid=5/mode=620) — sem isso o tswl
  falha com `falha ao abrir PTY`.
- `/dev/shm → /run/shm` no `start-gui.sh` (wlroots falhava o keymap shm).

Bug conhecido já resolvido (não repetir): o buffer de carregamento do kernel/initrd
não pode ser reaproveitado como área do código de setup do kernel (um sobrescreve o outro).

## Interface Gráfica (GUI)

STATUS: FUNCIONAL NO BOOT REAL (2026-09-07) — compositor como sessão
principal via DRM/KMS no initramfs, verificado headless (QEMU+QMP) e
pelo usuário (`make run-gui`).

O que funciona (testado e confirmado em 2026-09-07):
- Boot até desktop: 13 ícones, painel (CPU/MEM/relógio com segundos),
  taskbar (MENU, janelas, NET, relógio), wallpaper PNG, cursor SWL
  (55, com setas de resize) — tudo renderizado.
- Clique em ícone lança o app; tswl e swlpad compilados i386 no chroot
  (`scripts/build-gui-i386.sh` → `gui-artifacts/apps/`) e instalados
  em `/bin` (`build-gui-rootfs.sh`); tswl abre com shell funcional.
- Arrastar ícones (com limiar anti-clique), botão-direito com menu de
  contexto (Abrir / Remover do desktop / Restaurar ícones removidos).
- Resize server-side por borda/canto (`SWL_RESIZE_MARGIN`) + cursor
  visual; arrastar título desmaximiza.
- Menu iniciar, maximizar/minimizar, fechar pela decoração (herdados).

Em investigação (outras IAs, 2026-09-07):
- Ícones PNG do desktop caem no glifo vetorial (forma difere do asset;
  wallpaper carrega — falha específica dos ícones).
- Cursor de resize "gruda" dentro da janela se o reset pra `default`
  for removido (fix aplicado e confirmado pelo usuário; ver sessão).
- 1 segfault em libxkbcommon aos ~60 s de uso, sem reprodução até aqui.

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
- Lista de janelas abertas na taskbar: mostra título e foca ao
  clicar, mas sem preview/thumbnail.
- Janela maximizada não readapta o tamanho se o output for
  redimensionado depois (ver sessão de maximizar/minimizar).
- Fullscreen real: só responde ao protocolo, sem lógica de verdade.
- Entradas do menu iniciar não têm ícones (só texto).
- Não roda ainda no hardware real — só testado em QEMU.

## Linguagem SWL / Compilador swlc

STATUS: NÃO INICIADO.

## Aplicativos

STATUS: FUNCIONAL NO BOOT REAL (2026-09-07).

- **TSWL** (`apps/tswl/`): terminal nativo (wayland-client + xdg-shell
  + cairo/pango, parser ANSI próprio, PTY via `forkpty`). Compilado
  i386 no chroot (`scripts/build-gui-i386.sh` → `gui-artifacts/apps/`)
  e instalado em `/bin` — abre no boot real com shell funcional
  (exigia `/dev/pts` montado no init; ver sessão 2026-09-07).
- **SWLPAD** (`apps/swlpad/`): mesmo fluxo de build/instalação; abre.
- Os outros 11 itens do catálogo continuam placeholders (comando
  aponta pra binário inexistente).

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
1. Ícones PNG do desktop (caem no glifo vetorial — em investigação).
2. Readaptar janelas maximizadas quando o output redimensiona.
3. Fullscreen real (protocolo já responde, falta lógica).
