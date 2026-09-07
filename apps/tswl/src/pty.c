/*
 * pty.c — pseudo-terminal do TSWL.
 * forkpty() cuida de abrir o par mestre/escravo, setsid e TIOCSCTTY.
 */

#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <sys/ioctl.h>
#include "tswl_pty.h"

int tswl_pty_spawn(int cols, int rows, pid_t *child_pid) {
    int master = -1;
    struct winsize ws = {
        .ws_col = (unsigned short)cols,
        .ws_row = (unsigned short)rows,
        .ws_xpixel = 0,
        .ws_ypixel = 0,
    };
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        const char *shell = getenv("SHELL");
        if (!shell || !shell[0]) {
            shell = "/bin/sh";
        }
        setenv("TERM", "xterm-256color", 1);
        execl(shell, shell, (void *)NULL);
        _exit(1);
    }
    if (child_pid) {
        *child_pid = pid;
    }
    return master;
}

void tswl_pty_resize(int fd, int cols, int rows) {
    struct winsize ws = {
        .ws_col = (unsigned short)cols,
        .ws_row = (unsigned short)rows,
        .ws_xpixel = 0,
        .ws_ypixel = 0,
    };
    ioctl(fd, TIOCSWINSZ, &ws);
}
