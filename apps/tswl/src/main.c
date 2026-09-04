/*
 * main.c — TSWL, terminal nativo do SWL OS.
 *
 * Cliente Wayland puro: xdg-shell pra janela, shm buffers ARGB8888 pra
 * pixels, xkbcommon pra teclado. Um único loop de eventos via
 * wl_display_dispatch + poll() no fd do display e no fd mestre do PTY
 * (sem threads).
 *
 * Fluxo:
 *   teclado → xkb → sequência de terminal → write(pty)
 *   PTY (saída do shell) → term_feed → render_draw → wl_surface attach
 *   blink do cursor: wl_display_dispatch com timeout de 500ms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "term.h"
#include "tswl_pty.h"
#include "render.h"

#define INIT_WIDTH  660
#define INIT_HEIGHT 410
#define BLINK_MS    500

struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct wl_seat *seat;

    struct wl_surface *surface;
    struct xdg_surface *xsurface;
    struct xdg_toplevel *toplevel;
    struct wl_keyboard *keyboard;

    struct xkb_context *xkb_ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *xkb_state;

    struct wl_buffer *buffer;
    void *buffer_data;
    size_t buffer_size;      /* tamanho real do mmap (pra desmapear certo) */
    int width, height;       /* pixels, configurado pelo compositor */
    bool configured;
    bool running;

    tswl_term *term;
    tswl_render *render;
    int pty_fd;
    pid_t child_pid;
    bool need_redraw;
    bool cursor_on;
    long blink_ms_last;
};

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ---------------- buffer shm ---------------- */

static int create_shm_file(size_t size) {
    char name[] = "/tswl-XXXXXX";
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return -1;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
    (void)data;
    (void)buffer;
    /* buffer único reutilizado: o compositor avisa quando terminou de ler;
     * como sempre esperamos frame_done+release antes do próximo attach,
     * aqui não precisamos fazer nada. */
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static bool recreate_buffer(struct app *a) {
    if (a->buffer) {
        wl_buffer_destroy(a->buffer);
        a->buffer = NULL;
    }
    if (a->buffer_data) {
        /* usa o tamanho guardado na criação — a->width/height podem já ter
         * sido atualizados pro novo tamanho, e munmap com tamanho errado
         * corrompe o espaço de endereçamento (crash no resize) */
        munmap(a->buffer_data, a->buffer_size);
        a->buffer_data = NULL;
    }
    size_t stride = (size_t)a->width * 4;
    size_t size = stride * a->height;
    int fd = create_shm_file(size);
    if (fd < 0) return false;
    a->buffer_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (a->buffer_data == MAP_FAILED) {
        a->buffer_data = NULL;
        close(fd);
        return false;
    }
    a->buffer_size = size;
    struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, (int)size);
    a->buffer = wl_shm_pool_create_buffer(pool, 0, a->width, a->height,
                                          (int)stride, WL_SHM_FORMAT_ARGB8888);
    wl_buffer_add_listener(a->buffer, &buffer_listener, a);
    wl_shm_pool_destroy(pool);
    close(fd);
    return true;
}

/* ---------------- desenho ---------------- */

static void redraw(struct app *a) {
    tswl_render_draw(a->render, a->term, a->cursor_on);
    cairo_surface_t *surf = tswl_render_surface(a->render);
    int sw = cairo_image_surface_get_width(surf);
    int sh = cairo_image_surface_get_height(surf);
    unsigned char *src = cairo_image_surface_get_data(surf);
    int sstride = cairo_image_surface_get_stride(surf);
    int copy_w = sw < a->width ? sw : a->width;
    int copy_h = sh < a->height ? sh : a->height;
    for (int y = 0; y < copy_h; y++) {
        memcpy((char *)a->buffer_data + (size_t)y * a->width * 4,
               src + (size_t)y * sstride, (size_t)copy_w * 4);
    }
    wl_surface_attach(a->surface, a->buffer, 0, 0);
    wl_surface_damage_buffer(a->surface, 0, 0, a->width, a->height);
    wl_surface_commit(a->surface);
    a->need_redraw = false;
}

/* ---------------- xdg toplevel ---------------- */

static void xsurface_configure(void *data, struct xdg_surface *xs, uint32_t serial) {
    struct app *a = data;
    xdg_surface_ack_configure(xs, serial);
    a->configured = true;
    if (!a->buffer && !recreate_buffer(a)) {
        fprintf(stderr, "tswl: falha ao criar buffer shm\n");
        a->running = false;
        return;
    }
    a->need_redraw = true;
}

static const struct xdg_surface_listener xsurface_listener = {
    .configure = xsurface_configure,
};

static void toplevel_configure(void *data, struct xdg_toplevel *tl,
        int32_t w, int32_t h, struct wl_array *states) {
    struct app *a = data;
    (void)tl; (void)states;
    if (w > 0 && h > 0 && (w != a->width || h != a->height)) {
        a->width = w;
        a->height = h;
        /* recria render+grid no novo tamanho */
        tswl_render_free(a->render);
        a->render = tswl_render_new(w, h);
        int cols = tswl_render_cols_for(a->render, w);
        int rows = tswl_render_rows_for(a->render, h);
        tswl_term_resize(a->term, cols, rows);
        tswl_pty_resize(a->pty_fd, cols, rows);
        if (a->configured) {
            recreate_buffer(a);
            a->need_redraw = true;
        }
    }
}

static void toplevel_close(void *data, struct xdg_toplevel *tl) {
    (void)tl;
    struct app *a = data;
    a->running = false;
}

static void toplevel_configure_bounds(void *data, struct xdg_toplevel *tl,
        int32_t w, int32_t h) {
    (void)data; (void)tl; (void)w; (void)h;
}

static void toplevel_wm_capabilities(void *data, struct xdg_toplevel *tl,
        struct wl_array *caps) {
    (void)data; (void)tl; (void)caps;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
    .wm_capabilities = toplevel_wm_capabilities,
};

/* ---------------- teclado ---------------- */

static void keyboard_keymap(void *data, struct wl_keyboard *kb,
        uint32_t format, int32_t fd, uint32_t size) {
    struct app *a = data;
    (void)kb;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }
    struct xkb_keymap *km = xkb_keymap_new_from_string(a->xkb_ctx, map,
        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    close(fd);
    if (!km) return;
    struct xkb_state *st = xkb_state_new(km);
    if (!st) {
        xkb_keymap_unref(km);
        return;
    }
    if (a->keymap) xkb_keymap_unref(a->keymap);
    if (a->xkb_state) xkb_state_unref(a->xkb_state);
    a->keymap = km;
    a->xkb_state = st;
}

static void keyboard_enter(void *data, struct wl_keyboard *kb,
        uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    (void)data; (void)kb; (void)serial; (void)surface; (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *kb,
        uint32_t serial, struct wl_surface *surface) {
    (void)data; (void)kb; (void)serial; (void)surface;
}

static void keyboard_modifiers(void *data, struct wl_keyboard *kb,
        uint32_t serial, uint32_t depressed, uint32_t latched,
        uint32_t locked, uint32_t group) {
    struct app *a = data;
    (void)kb; (void)serial;
    if (a->xkb_state) {
        xkb_state_update_mask(a->xkb_state, depressed, latched, locked, 0, 0, group);
    }
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *kb,
        int32_t rate, int32_t delay) {
    (void)data; (void)kb; (void)rate; (void)delay;
}

/* traduz keysym pra sequência de terminal; retorna bytes escritos em buf */
static int keysym_to_seq(xkb_keysym_t sym, char *buf, bool app_cursor) {
    struct { xkb_keysym_t sym; const char *norm; const char *app; } keys[] = {
        { XKB_KEY_Up,        "\033[A", "\033OA" },
        { XKB_KEY_Down,      "\033[B", "\033OB" },
        { XKB_KEY_Right,     "\033[C", "\033OC" },
        { XKB_KEY_Left,      "\033[D", "\033OD" },
        { XKB_KEY_Home,      "\033[H", "\033OH" },
        { XKB_KEY_End,       "\033[F", "\033OF" },
        { XKB_KEY_Insert,    "\033[2~", NULL },
        { XKB_KEY_Delete,    "\033[3~", NULL },
        { XKB_KEY_Page_Up,   "\033[5~", NULL },
        { XKB_KEY_Page_Down, "\033[6~", NULL },
        { XKB_KEY_BackSpace, "\177",   NULL },
        { XKB_KEY_Escape,    "\033",   NULL },
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (keys[i].sym == sym) {
            const char *s = (app_cursor && keys[i].app) ? keys[i].app : keys[i].norm;
            int n = (int)strlen(s);
            memcpy(buf, s, (size_t)n);
            return n;
        }
    }
    return 0;
}

static void keyboard_key(void *data, struct wl_keyboard *kb,
        uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    struct app *a = data;
    (void)kb; (void)serial; (void)time;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !a->xkb_state) return;
    if (a->pty_fd < 0) return;

    uint32_t keycode = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(a->xkb_state, keycode);
    char buf[64];
    int n = 0;

    xkb_mod_mask_t ctrl = xkb_state_mod_name_is_active(a->xkb_state,
        "Control", XKB_STATE_MODS_EFFECTIVE);
    xkb_mod_mask_t shift = xkb_state_mod_name_is_active(a->xkb_state,
        "Shift", XKB_STATE_MODS_EFFECTIVE);

    /* Shift+PageUp/Down: scrollback do terminal (não vai pro shell) */
    if (shift && sym == XKB_KEY_Page_Up) {
        tswl_term_scroll_view(a->term, tswl_term_rows(a->term) / 2);
        a->need_redraw = true;
        return;
    }
    if (shift && sym == XKB_KEY_Page_Down) {
        tswl_term_scroll_view(a->term, -tswl_term_rows(a->term) / 2);
        a->need_redraw = true;
        return;
    }

    /* Ctrl+letra → byte de controle (Ctrl+C = 0x03 etc.) */
    if (ctrl && sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
        buf[0] = (char)(sym - XKB_KEY_a + 1);
        n = 1;
    } else if (ctrl && sym >= XKB_KEY_A && sym <= XKB_KEY_Z) {
        buf[0] = (char)(sym - XKB_KEY_A + 1);
        n = 1;
    } else if (ctrl && sym == XKB_KEY_space) {
        buf[0] = 0;
        n = 1;
    } else {
        n = keysym_to_seq(sym, buf, false);
        if (n == 0) {
            /* tecla imprimível: UTF-8 direto do xkb */
            n = xkb_state_key_get_utf8(a->xkb_state, keycode, buf, sizeof(buf));
            if (ctrl && n > 0) {
                /* Ctrl+tecla não-imprimível mapeada acima; aqui ignora */
                n = 0;
            }
        }
    }

    if (n > 0) {
        ssize_t w = write(a->pty_fd, buf, (size_t)n);
        (void)w;
        tswl_term_scroll_view(a->term, -tswl_term_rows(a->term));  /* digitar volta pro fim */
    }
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

/* ---------------- seat / registry ---------------- */

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *a = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) {
        wl_keyboard_destroy(a->keyboard);
        a->keyboard = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data; (void)seat; (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    struct app *a = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        a->compositor = wl_registry_bind(registry, name,
            &wl_compositor_interface, version < 4 ? version : 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        /* o swlwm cria o xdg_shell com versão 3; bindar abaixo disso faz
         * o wlroots não enviar o configure inicial pra esse cliente */
        a->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
            version < 3 ? version : 3);
        xdg_wm_base_add_listener(a->wm_base, &wm_base_listener, a);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        a->seat = wl_registry_bind(registry, name, &wl_seat_interface,
            version < 5 ? version : 5);
        wl_seat_add_listener(a->seat, &seat_listener, a);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

/* ---------------- main ---------------- */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    struct app a = {0};
    a.width = INIT_WIDTH;
    a.height = INIT_HEIGHT;
    a.running = true;
    a.cursor_on = true;
    a.pty_fd = -1;
    a.blink_ms_last = now_ms();

    a.display = wl_display_connect(NULL);
    if (!a.display) {
        fprintf(stderr, "tswl: não consegui conectar no Wayland (WAYLAND_DISPLAY=%s)\n",
            getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(vazio)");
        return 1;
    }
    a.registry = wl_display_get_registry(a.display);
    wl_registry_add_listener(a.registry, &registry_listener, &a);
    wl_display_roundtrip(a.display);

    if (!a.compositor || !a.shm || !a.wm_base) {
        fprintf(stderr, "tswl: compositor/shm/xdg-shell indisponível\n");
        return 1;
    }

    /* render + grid no tamanho inicial */
    a.render = tswl_render_new(a.width, a.height);
    if (!a.render) {
        fprintf(stderr, "tswl: falha ao criar render\n");
        return 1;
    }
    int cols = tswl_render_cols_for(a.render, a.width);
    int rows = tswl_render_rows_for(a.render, a.height);
    a.term = tswl_term_new(cols, rows);
    if (!a.term) {
        fprintf(stderr, "tswl: falha ao criar terminal\n");
        return 1;
    }

    /* PTY com o shell */
    a.pty_fd = tswl_pty_spawn(cols, rows, &a.child_pid);
    if (a.pty_fd < 0) {
        fprintf(stderr, "tswl: falha ao abrir PTY\n");
        return 1;
    }

    /* janela */
    a.surface = wl_compositor_create_surface(a.compositor);
    a.xsurface = xdg_wm_base_get_xdg_surface(a.wm_base, a.surface);
    xdg_surface_add_listener(a.xsurface, &xsurface_listener, &a);
    a.toplevel = xdg_surface_get_toplevel(a.xsurface);
    xdg_toplevel_add_listener(a.toplevel, &toplevel_listener, &a);
    xdg_toplevel_set_title(a.toplevel, "TSWL");
    xdg_toplevel_set_app_id(a.toplevel, "tswl");
    wl_surface_commit(a.surface);
    wl_display_roundtrip(a.display);  /* processa o primeiro configure */

    a.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    /* loop principal: Wayland fd + PTY fd, com timeout do blink */
    int wl_fd = wl_display_get_fd(a.display);
    while (a.running) {
        /* processa eventos Wayland já recebidos */
        if (wl_display_dispatch_pending(a.display) == -1) {
            break;
        }
        wl_display_flush(a.display);

        int timeout = BLINK_MS - (int)(now_ms() - a.blink_ms_last);
        if (timeout < 0) timeout = 0;

        struct pollfd fds[2];
        fds[0].fd = wl_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = a.pty_fd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int nready = poll(fds, 2, timeout);

        if (nready > 0 && (fds[0].revents & POLLIN)) {
            /* lê e processa os eventos Wayland que chegaram */
            if (wl_display_dispatch(a.display) == -1) {
                break;
            }
        }

        if (nready > 0 && (fds[1].revents & (POLLIN | POLLHUP))) {
            char buf[8192];
            ssize_t n = read(a.pty_fd, buf, sizeof(buf));
            if (n > 0) {
                if (tswl_term_feed(a.term, buf, (size_t)n)) {
                    a.need_redraw = true;
                }
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EIO)) {
                /* shell morreu: fecha o terminal junto */
                a.running = false;
            } else if (n < 0 && errno == EIO) {
                a.running = false;
            }
        }

        /* blink do cursor */
        if (now_ms() - a.blink_ms_last >= BLINK_MS) {
            a.cursor_on = !a.cursor_on;
            a.blink_ms_last = now_ms();
            a.need_redraw = true;
        }

        if (a.need_redraw && a.configured && a.buffer) {
            redraw(&a);
        }
    }

    /* limpeza */
    if (a.child_pid > 0) {
        kill(a.child_pid, SIGHUP);
        waitpid(a.child_pid, NULL, WNOHANG);
    }
    if (a.pty_fd >= 0) close(a.pty_fd);
    if (a.buffer) wl_buffer_destroy(a.buffer);
    if (a.buffer_data) munmap(a.buffer_data, a.buffer_size);
    if (a.keyboard) wl_keyboard_destroy(a.keyboard);
    if (a.keymap) xkb_keymap_unref(a.keymap);
    if (a.xkb_state) xkb_state_unref(a.xkb_state);
    if (a.xkb_ctx) xkb_context_unref(a.xkb_ctx);
    tswl_term_free(a.term);
    tswl_render_free(a.render);
    xdg_toplevel_destroy(a.toplevel);
    xdg_surface_destroy(a.xsurface);
    wl_surface_destroy(a.surface);
    wl_display_disconnect(a.display);
    return 0;
}
