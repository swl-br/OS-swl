# AVISO — tswl-catalog: rebase necessário (2026-09-09, orquestrador)

## Veredito: OBJETIVO APROVADO, arquivos BLOQUEADOS por base velha

Resolver "tswl: not found" ao clicar no ícone é válido e as 4 adições
são boas. Mas os arquivos vieram de base anterior a C1, R-13 e T3 —
subir apagaria os três. NÃO integrar como está.

## O que fazer (só adições, sobre os arquivos atuais)

1. `swl-ui/src/desktop.c`: catálogo com `/bin/tswl` e `/bin/swlpad`
   (2 linhas, sem tirar `find_free_slot`/`top_offset`).
2. `swl-ui/src/swlwm.c`: helper `swl_launch_command()` (fork + PATH
   mínimo + `execl /bin/sh -c`) + trocar os 3 pontos de fork/execl
   (ícone desktop, menu, contexto) pra usar o helper. Sem tirar
   `resize_pending`, `move_snap`, margens, `find_free_slot`, R-13.
3. `userland/build-gui-rootfs.sh`: bloco de checagem `bin/tswl`
   (após o loop de cópia de apps) + `export PATH`/`XDG_RUNTIME_DIR`
   no `start-gui.sh` (após as linhas de `/dev/shm`). Sem tirar
   `fonts.conf` do T3.

Status: PENDENTE.
