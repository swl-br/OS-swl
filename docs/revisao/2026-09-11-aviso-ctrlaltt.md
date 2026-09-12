# AVISO — ctrl-alt-t: manter show-desktop (2026-09-11, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA por base velha

Ctrl+Alt+T via launcher está certo. Mas o arquivo não tem
show-desktop (`show_desktop_active`, `toggle_show_desktop` — entraram
no `main` depois da base usada). Subir apaga Super+D.

## O que fazer

Somar o bloco Ctrl+Alt+T sobre o `swlwm.c` atual (que já tem
show-desktop + snap + F-keys). Nada a remover.

Status: PENDENTE.
