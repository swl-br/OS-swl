# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: V1 — identidade visual do terminal (pacote shell)

## Escopo

- `userland/shell/shell-rc.sh` (novo)
- `userland/shell/etc-profile` (novo)
- `userland/build-rootfs.sh` (instala no rootfs)
- `userland/swlfetch` (paths de arte)
- `apps/tswl/src/pty.c` (`ENV` pro ash interativo)

Não toca: swlc, resize Claude.

## O que entrega

| Item | Como |
|------|------|
| Splash ao abrir terminal | `shell-rc.sh` roda `swlfetch` se tty |
| Prompt `SWL:~$` | `PS1` |
| Help | `swl_help` |
| Clear com cabeçalho | `swl_clear` |
| Spinner / progresso | `swl_spinner N`, `swl_progress N` |
| Instalação rootfs | `build-rootfs.sh` → `/usr/share/swl/*`, `/bin/swlfetch`, `/etc/profile` |
| TSWL pega o rc | `setenv("ENV", ...)` no child do PTY |

## Teste

```bash
# no host (sintaxe)
sh -n userland/shell/shell-rc.sh userland/swlfetch userland/build-rootfs.sh

# no sistema (após rebuild rootfs + disco)
# abrir tswl → splash Neo + prompt SWL:~$
# swl_help ; swl_clear ; swl_spinner 1 ; swl_progress 5
```
