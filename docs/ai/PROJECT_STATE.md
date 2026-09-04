# PROJECT_STATE — SWL OS

Última atualização: 2026-09-03 (sessão com Claude)

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
├── desktop.c/h          (ícones da área de trabalho)
├── swl_buffer.c/h       (integração cairo → wlr_scene_buffer)
└── swl_draw_util.c/h    (utilitários de desenho)
```

Build: `meson setup build && ninja -C build`
Teste (dentro de sessão gráfica X11 existente, sem sair do host):
`WLR_BACKEND=x11 ./build/swlwm -s "foot"`
(cliente de teste precisa ser Wayland nativo — `xterm` NÃO funciona,
só engana visualmente por rodar fora do nosso compositor)

### O que já funciona (testado e confirmado)
- Fundo, painel superior (com CPU/MEM reais, relógio), taskbar, ícones
  visuais na área de trabalho — tudo desenhado corretamente.
- Redimensionar a janela de teste: painel/taskbar/fundo se recalculam
  certo (bug corrigido nesta sessão — antes só recalculava na criação).
- Arrastar janela pela barra de título própria: funcional (bug de
  crash corrigido nesta sessão).
- Fechar janela pelo botão da decoração: funcional.

### Bugs corrigidos nesta sessão (2026-09-03, Claude)
1. **Redimensionamento não propagava**: `output_request_state()` só
   confirmava o novo estado do output (`wlr_output_commit_state`), mas
   nunca chamava `swl_background_resize`/`swl_panel_resize`/
   `swl_taskbar_resize`. Corrigido: agora recalcula o layout sempre que
   a resolução efetiva do output muda.
2. **Crash (segfault) ao arrastar janela pela barra de título própria**:
   em `begin_interactive()`, a checagem `toplevel->...surface !=
   wlr_surface_get_root_surface(focused_surface)` não tratava o caso de
   `focused_surface == NULL` — que é exatamente o que acontece quando o
   clique inicial vem da NOSSA decoração (não é uma superfície de
   cliente real, então o seat não tem foco de superfície ali). Chamar
   `wlr_surface_get_root_surface(NULL)` gerava o crash. Corrigido:
   pula essa checagem quando `focused_surface == NULL` (já sabemos
   qual toplevel mover, veio do hit-test da decoração).

### O que NÃO está implementado ainda (não fingir que está pronto)
- Maximizar/minimizar janela: botões existem na decoração (visual),
  mas o clique neles ainda não faz nada real (TODO no código).
- Menu (botão "MENU" no painel/taskbar): só o botão existe, sem
  conteúdo/lista de apps ao clicar.
- Trocar papel de parede: não implementado.
- Arrastar ícones da área de trabalho, adicionar/remover atalhos: não
  implementado (ícones são fixos, não-interativos além do clique que
  já abre programas).
- Lista de janelas abertas na taskbar: não implementado.
- Não roda ainda no hardware real / initramfs do SWL OS — só testado
  na sessão gráfica do Xubuntu do desenvolvedor (`WLR_BACKEND=x11`).
  Integração com o boot real (rodar como o processo gráfico principal
  dentro do initramfs, sem X11 por baixo) ainda não foi feita.

## Linguagem SWL / Compilador swlc

STATUS: NÃO INICIADO.

## Aplicativos

STATUS: NÃO INICIADO (fora do escopo desta sessão; outras IAs podem
estar trabalhando nisso em paralelo — verificar `docs/ai/sessions/`
antes de assumir que algo não existe).

## Próximos passos sugeridos (GUI)
1. Maximizar/minimizar funcional.
2. Conteúdo real do menu.
3. Integrar o compositor ao boot real (substituir o teste `WLR_BACKEND=x11`
   por rodar como sessão gráfica principal via DRM/KMS, dentro do
   ambiente do SWL OS de verdade).
