# Sessão 2026-09-07 — recuperação da pasta oficial + boot GUI ponta a ponta
(Orquestrador/admin — sem mexer em código alheio em curso.)

## O que aconteceu

1. **Pasta oficial apagada pelo usuário** (`SWL-OS`, com o `.git` e o
   trabalho recente). Reconstruída a partir dos snapshots em
   `~/Documentos/Dev/OS-swl*` + re-aplicação do histórico de sessão:
   - `swl-ui` ouro = `Dev/OS-swl` (trabalho do Claude até 00:13: ctx
     menu, drag de ícones, `panel.c` com CPU/MEM/relógio);
   - re-aplicados por cima: trigger de resize + cursor visual
     (`SWL_RESIZE_MARGIN`), aliases de borda no tema de cursor (55),
     GUI-012 (icons-pack), regra de initramfs no Makefile.
2. **Primeiro boot deu panic** (`Initramfs unpacking failed`): a causa
   NÃO era o `cpio` — era o `stage2.asm` carregando o initrd colado no
   kernel (~6,5 MB), onde o kernel se autodescomprime (~20–30 MB) e
   pisa na cauda. Initrds ≤13 MB sobreviviam por terminarem antes.
   **Fix: initrd no topo da RAM (`INITRD_TOP 0x17000000`)**, como o GRUB
   faz. Comprovado: o initramfs de 18,9 MB do fluxo original falhava
   igual no endereço baixo e passou no alto.
3. **Boot calado após o init** (`c1 c2 c3`, sem shell): `/bin/sh` era um
   bash 64-bit do host, inexequível no kernel i386. **Fix: `sh →
   busybox` i386 estático** (+ mesma troca no `build-rootfs.sh`).
4. **swlwm segfault no boot**: binário de set-06-01:42 morria no
   libxkbcommon sem `/usr/share/X11/xkb`. **Fix: binário de set-05
   (79600 B, tolera xkb ausente) + seção xkb-data no
   `build-gui-rootfs.sh` + `/dev/shm → /run/shm` no `start-gui.sh`**
   (sumiu o `Failed to allocate shm file for keymap`).
5. **tswl não abria** (`falha ao abrir PTY`): faltava montar `devpts`.
   **Fix: `mkdir /dev/pts` + `mount devpts gid=5,mode=620` no
   `init.asm`.** Verificado headless: janela do terminal abre com
   shell `/ #` funcional.
6. **Apps fora do rootfs**: `build-gui-i386.sh` nunca compilou
   tswl/swlpad (e os binários em `apps/*/build/` são x86_64 do host).
   Estendido: chroot compila os apps i386 → `gui-artifacts/apps/` →
   `build-gui-rootfs.sh` instala em `/bin`.
7. **Push de segurança**: `.git` novo, `.gitignore` real, commit
   `e78d16a` (969 arquivos, só fontes), `kernel/config/swl_defconfig`
   no path canônico do `fetch-deps.sh`, `build-initramfs.sh` (R-16)
   recuperado do `origin/main`. Push na branch `gui-end-to-end`,
   depois **fast-forward do `main` com backup** (tag `main-arquivo`
   preserva as 54 commits antigas). Limpeza: tarballs redundantes +
   `cat.png` removidos.

## Matriz verificada (QEMU headless + QMP, binário 88876)
- Desktop: 13 ícones + painel + taskbar renderizam; wallpaper PNG ok.
- Clique abre apps (execl funciona); drag move ícones.
- Botão-direito abre ctx menu (Abrir/Remover); remover esconde
  (brilho 176→0); restaurar traz de volta. (Nota de harness: no HMP
  deste QEMU, botão direito = `mouse_button 2`.)
- Resize no binário (trigger + cursor visual); teste com janela ficou
  pro usuário (precisava dos apps).
- Teclado não crasha (4 teclas ok).

## Pendências deixadas em curso (outras IAs, 2026-09-07)
- **Ícones PNG caem no glifo vetorial** (confirmado por IoU 0,04 entre
  tela e asset; wallpaper carrega — falha específica dos ícones).
  Debug `swl-desktop: png ... status=` adicionado ao `desktop.c` e
  depois removido por outra IA durante a investigação (ver diff atual).
- **Cursor de resize "gruda" dentro da janela**: fix com reset pra
  `default` foi aplicado, confirmado pelo usuário e **depois removido
  por outra IA** (ver `git diff swl-ui/src/swlwm.c`) — REGRESSÃO
  sinalizada, aguardando decisão (reaplicar).
- Possível segfault xkbcommon aos ~60 s de uso (1 ocorrência, sem
  reprodução; shm fix aplicado depois — retestar).
- `fetch-deps.sh` gera bash/busybox do host (x86_64): num clone limpo
  em máquina 64-bit, `/bin/sh` nasce quebrado de novo (ver fix §3).
