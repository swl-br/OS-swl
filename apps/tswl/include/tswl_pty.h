#ifndef TSWL_TSWL_PTY_H
#define TSWL_TSWL_PTY_H

#include <sys/types.h>

/*
 * pty: fork + exec do shell dentro de um pseudo-terminal.
 * Uma única chamada de setup; o fd retornado vai pro mesmo poll() do
 * loop Wayland (sem thread extra).
 */

/* Abre um PTY, faz fork; no filho, exec /bin/sh (ou $SHELL) com
 * TERM=xterm-256color e o tamanho inicial cols x rows. No pai, retorna
 * o fd mestre e o pid do filho em *child_pid. Retorna -1 em erro. */
int tswl_pty_spawn(int cols, int rows, pid_t *child_pid);

/* Repassa o novo tamanho da janela pro PTY (SIGWINCH automático). */
void tswl_pty_resize(int fd, int cols, int rows);

#endif /* TSWL_TSWL_PTY_H */
