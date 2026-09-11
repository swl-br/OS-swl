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
#define TSWL_BUF_COUNT 2  /* double-buffer: um no compositor, um livre pra pintar */

struct app; /* forward */

/* Um slot de shm. O compositor segura o wl_buffer até mandar
 * wl_buffer.release; só então podemos reescrever ou destruir (R-06). */
struct tswl_shm_buf {
    struct app *app; /* pra recriar o slot no release se ficou stale */
    struct wl_buffer *wl;
    void *data;
    size_t size;
    int buf_w, buf_h;  /* dims da criação; pick só reusa se bate atual */
    bool busy;   /* attach feito, esperando release */
    bool stale;  /* resize pediu destruição enquanto busy */
};

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
    struct wl_pointer *pointer;
    int pointer_x, pointer_y;

    struct xkb_context *xkb_ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *xkb_state;

    struct tswl_shm_buf bufs[TSWL_BUF_COUNT];
    int width, height;       /* pixels, configurado pelo compositor */
    bool configured;
    bool running;

    tswl_term *term;
    tswl_render *render;
    swl_menubar *menubar;
    int pty_fd;
    pid_t child_pid;
    bool need_redraw;
    bool cursor_on;
    long blink_ms_last;

    /* T5: selecao + clipboard interno */
    bool selecting;
    int sel_anchor_c, sel_anchor_r;
    long last_click_ms;
    int last_click_c, last_click_r;
    int click_count;
    long bell_until_ms;
    char *clipboard;
    size_t clipboard_len;
};

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ---------------- buffer shm (double-buffer, R-06) ---------------- */

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

static void shm_buf_free_resources(struct tswl_shm_buf *b) {
    if (b->wl) {
        wl_buffer_destroy(b->wl);
        b->wl = NULL;
    }
    if (b->data) {
        munmap(b->data, b->size);
        b->data = NULL;
    }
    b->size = 0;
    b->busy = false;
    b->stale = false;
}

static bool shm_buf_create(struct app *a, struct tswl_shm_buf *b,
        int width, int height);

static void buffer_release(void *data, struct wl_buffer *buffer) {
    struct tswl_shm_buf *b = data;
    (void)buffer;
    b->busy = false;
    /* Resize pediu destruição enquanto o compositor ainda lia:
     * agora que liberou, destrói o buffer antigo e recria no tamanho
     * atual da janela (se ainda fizer sentido). */
    if (b->stale) {
        struct app *a = b->app;
        shm_buf_free_resources(b);
        if (a && a->width > 0 && a->height > 0 && a->shm) {
            (void)shm_buf_create(a, b, a->width, a->height);
        }
    }
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static bool shm_buf_create(struct app *a, struct tswl_shm_buf *b,
        int width, int height) {
    size_t stride = (size_t)width * 4;
    size_t size = stride * (size_t)height;
    int fd = create_shm_file(size);
    if (fd < 0) return false;
    void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return false;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, (int)size);
    struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, width, height,
            (int)stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    if (!wl) {
        munmap(map, size);
        return false;
    }
    b->app = a;
    b->wl = wl;
    b->data = map;
    b->size = size;
    b->buf_w = width;
    b->buf_h = height;
    b->busy = false;
    b->stale = false;
    wl_buffer_add_listener(b->wl, &buffer_listener, b);
    return true;
}

static struct tswl_shm_buf *pick_free_buf(struct app *a) {
    for (int i = 0; i < TSWL_BUF_COUNT; i++) {
        struct tswl_shm_buf *b = &a->bufs[i];
        if (b->wl && b->data && !b->busy && !b->stale
            && b->buf_w == a->width && b->buf_h == a->height) {
            return b;
        }
    }
    return NULL;
}

static bool has_any_buffer(struct app *a) {
    for (int i = 0; i < TSWL_BUF_COUNT; i++) {
        if (a->bufs[i].wl) {
            return true;
        }
    }
    return false;
}

/* (Re)cria buffers no tamanho atual. Slots ainda busy ficam marcados
 * stale e só são trocados no release — nunca destruímos um wl_buffer
 * que o compositor ainda está lendo (R-06). */
static bool recreate_buffers(struct app *a) {
    if (a->width <= 0 || a->height <= 0) {
        return false;
    }
    bool any_busy_deferred = false;
    bool any_created = false;
    for (int i = 0; i < TSWL_BUF_COUNT; i++) {
        struct tswl_shm_buf *b = &a->bufs[i];
        if (b->busy) {
            b->stale = true;  /* troca real no release */
            any_busy_deferred = true;
            continue;
        }
        shm_buf_free_resources(b);
        if (shm_buf_create(a, b, a->width, a->height)) {
            any_created = true;
        }
    }
    /* OK se tem slot livre agora, ou se só adiou (release recria). */
    return any_created || any_busy_deferred || pick_free_buf(a) != NULL;
}

/* ---------------- desenho ---------------- */

static void redraw(struct app *a) {
    struct tswl_shm_buf *b = pick_free_buf(a);
    if (!b) {
        /* Os dois slots estão no compositor (ou stale). Mantém
         * need_redraw pra tentar de novo no próximo ciclo do loop,
         * depois que algum release chegar. */
        return;
    }

    tswl_render_draw(a->render, a->term, a->menubar, a->cursor_on);
    if (a->bell_until_ms > 0 && now_ms() < a->bell_until_ms) {
        cairo_t *crb = cairo_create(tswl_render_surface(a->render));
        cairo_set_source_rgba(crb, 1.0, 1.0, 1.0, 0.35);
        cairo_paint(crb);
        cairo_destroy(crb);
        a->need_redraw = true;
    } else if (a->bell_until_ms > 0) {
        a->bell_until_ms = 0;
    }
    cairo_surface_t *surf = tswl_render_surface(a->render);
    int sw = cairo_image_surface_get_width(surf);
    int sh = cairo_image_surface_get_height(surf);
    unsigned char *src = cairo_image_surface_get_data(surf);
    int sstride = cairo_image_surface_get_stride(surf);
    int copy_w = sw < a->width ? sw : a->width;
    int copy_h = sh < a->height ? sh : a->height;
    size_t dst_stride = (size_t)a->width * 4;
    /* W1: limpa buffer antes — evita farelos de frame antigo */
    if (b->size > 0)
        memset(b->data, 0, b->size);
    for (int y = 0; y < copy_h; y++) {
        memcpy((char *)b->data + (size_t)y * dst_stride,
               src + (size_t)y * sstride, (size_t)copy_w * 4);
    }
    wl_surface_attach(a->surface, b->wl, 0, 0);
    wl_surface_damage_buffer(a->surface, 0, 0, a->width, a->height);
    wl_surface_commit(a->surface);
    b->busy = true;
    a->need_redraw = false;
}

/* ---------------- xdg toplevel ---------------- */

static void xsurface_configure(void *data, struct xdg_surface *xs, uint32_t serial) {
    struct app *a = data;
    xdg_surface_ack_configure(xs, serial);
    a->configured = true;
    if (!has_any_buffer(a) && !recreate_buffers(a)) {
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
    if (w <= 0 || h <= 0)
        return;
    /* W1: evita configure minusculo apos resize de output. */
    if (w < 160)
        w = 160;
    if (h < TSWL_MENUBAR_H + 2 * TSWL_RENDER_PAD + 32)
        h = TSWL_MENUBAR_H + 2 * TSWL_RENDER_PAD + 32;
    if (w == a->width && h == a->height)
        return;
    a->width = w;
    a->height = h;
    if (a->menubar)
        swl_menubar_resize(a->menubar, w);
    tswl_render_free(a->render);
    a->render = tswl_render_new(w, h);
    if (!a->render) {
        fprintf(stderr, "tswl: falha ao recriar render no resize\n");
        a->running = false;
        return;
    }
    int cols = tswl_render_cols_for(a->render, w);
    int rows = tswl_render_rows_for(a->render, h);
    if (cols < 10) cols = 10;
    if (rows < 3) rows = 3;
    tswl_term_resize(a->term, cols, rows);
    tswl_pty_resize(a->pty_fd, cols, rows);
    if (a->configured) {
        if (!recreate_buffers(a)) {
            fprintf(stderr, "tswl: falha ao recriar buffers no resize\n");
            a->running = false;
            return;
        }
        a->need_redraw = true;
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
    /* Defesa: se xkb_ctx ainda não existir (ordem de eventos), ignora
     * em vez de null-deref. O caminho normal cria o contexto antes do
     * primeiro roundtrip. */
    if (!a->xkb_ctx) {
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
        /* F1-F12 (VT100 / xterm common) — R-14 */
        { XKB_KEY_F1,  "\033OP",   NULL },
        { XKB_KEY_F2,  "\033OQ",   NULL },
        { XKB_KEY_F3,  "\033OR",   NULL },
        { XKB_KEY_F4,  "\033OS",   NULL },
        { XKB_KEY_F5,  "\033[15~", NULL },
        { XKB_KEY_F6,  "\033[17~", NULL },
        { XKB_KEY_F7,  "\033[18~", NULL },
        { XKB_KEY_F8,  "\033[19~", NULL },
        { XKB_KEY_F9,  "\033[20~", NULL },
        { XKB_KEY_F10, "\033[21~", NULL },
        { XKB_KEY_F11, "\033[23~", NULL },
        { XKB_KEY_F12, "\033[24~", NULL },
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

static void execute_action(struct app *a, int id);


/* T5 helpers (forward decls before keyboard_key uses them) */
#ifndef BTN_LEFT
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112
#endif

static void selection_copy(struct app *a);
static void clipboard_paste(struct app *a);

static void pixel_to_cell(struct app *a, int sx, int sy, int *col, int *row) {
    int cw = tswl_render_cell_w(a->render);
    int ch = tswl_render_cell_h(a->render);
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    /* grid comeca abaixo da menubar */
    int y = sy - TSWL_MENUBAR_H - TSWL_RENDER_PAD;
    int x = sx - TSWL_RENDER_PAD;
    int c = x / cw;
    int r = y / ch;
    int cols = tswl_term_cols(a->term);
    int rows = tswl_term_rows(a->term);
    if (c < 0) c = 0;
    if (r < 0) r = 0;
    if (c >= cols) c = cols - 1;
    if (r >= rows) r = rows - 1;
    *col = c;
    *row = r;
}

static void selection_copy(struct app *a) {
    char *txt = tswl_term_selection_text(a->term);
    if (txt) {
        free(a->clipboard);
        a->clipboard = txt;
        a->clipboard_len = strlen(txt);
    }
}

static void clipboard_paste(struct app *a) {
    if (!a->clipboard || a->clipboard_len == 0 || a->pty_fd < 0) return;
    /* Bracketed paste: se o shell pediu CSI ?2004h, envolve o texto
     * com ESC[200~ ... ESC[201~ pra o readline nao interpretar. */
    if (a->term && tswl_term_bracketed_paste(a->term)) {
        const char start[] = "\033[200~";
        const char end[] = "\033[201~";
        ssize_t w = write(a->pty_fd, start, sizeof(start) - 1);
        (void)w;
        w = write(a->pty_fd, a->clipboard, a->clipboard_len);
        (void)w;
        w = write(a->pty_fd, end, sizeof(end) - 1);
        (void)w;
    } else {
        ssize_t w = write(a->pty_fd, a->clipboard, a->clipboard_len);
        (void)w;
    }
    tswl_term_scroll_view(a->term, -tswl_term_rows(a->term));
    a->need_redraw = true;
}

static void keyboard_key(void *data, struct wl_keyboard *kb,
        uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    struct app *a = data;
    (void)kb; (void)serial; (void)time;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !a->xkb_state) return;
    if (a->pty_fd < 0) return;

    uint32_t keycode = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(a->xkb_state, keycode);

    if (!a->menubar) { /* sem menubar: segue o fluxo normal */ }
    else {
        xkb_mod_mask_t alt = xkb_state_mod_name_is_active(a->xkb_state,
            "Mod1", XKB_STATE_MODS_EFFECTIVE);
        bool alt_key = (sym == XKB_KEY_Alt_L) || (sym == XKB_KEY_Alt_R);
        if (alt_key || (alt && (
                sym == XKB_KEY_Left || sym == XKB_KEY_Right ||
                sym == XKB_KEY_Up || sym == XKB_KEY_Down ||
                sym == XKB_KEY_Return || sym == XKB_KEY_Escape))) {
            enum swl_menubar_key mk;
            if (alt_key) mk = SWL_MENUBAR_KEY_ALT;
            else if (sym == XKB_KEY_Left) mk = SWL_MENUBAR_KEY_LEFT;
            else if (sym == XKB_KEY_Right) mk = SWL_MENUBAR_KEY_RIGHT;
            else if (sym == XKB_KEY_Up) mk = SWL_MENUBAR_KEY_UP;
            else if (sym == XKB_KEY_Down) mk = SWL_MENUBAR_KEY_DOWN;
            else if (sym == XKB_KEY_Return) mk = SWL_MENUBAR_KEY_ENTER;
            else mk = SWL_MENUBAR_KEY_ESC;
            int id2 = swl_menubar_key(a->menubar, mk);
            if (id2 > 0) execute_action(a, id2);
            a->need_redraw = true;
            return;
        }
    }
    char buf[64];
    int n = 0;

    xkb_mod_mask_t ctrl = xkb_state_mod_name_is_active(a->xkb_state,
        "Control", XKB_STATE_MODS_EFFECTIVE);
    xkb_mod_mask_t shift = xkb_state_mod_name_is_active(a->xkb_state,
        "Shift", XKB_STATE_MODS_EFFECTIVE);

    /* T5: Ctrl+Shift+C copia, Ctrl+Shift+V cola */
    if (ctrl && shift && (sym == XKB_KEY_C || sym == XKB_KEY_c)) {
        if (tswl_term_has_selection(a->term))
            selection_copy(a);
        return;
    }
    if (ctrl && shift && (sym == XKB_KEY_V || sym == XKB_KEY_v)) {
        clipboard_paste(a);
        return;
    }

    /* T5+: Ctrl+Shift+A seleciona tudo */
    if (ctrl && shift && (sym == XKB_KEY_A || sym == XKB_KEY_a)) {
        if (a->term) {
            tswl_term_select_all(a->term);
            a->need_redraw = true;
        }
        return;
    }

    /* Shift+PageUp/Down: scrollback do terminal (nao vai pro shell) */
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
        n = keysym_to_seq(sym, buf, tswl_term_app_cursor(a->term));
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


/* menus da menubar */
enum { TSWL_ACTION_LIMPAR = 1, TSWL_ACTION_SAIR = 2 };

static const swl_menuitem tswl_arquivo[] = {
    { "Limpar", TSWL_ACTION_LIMPAR, true },
    { "Sair", TSWL_ACTION_SAIR, true },
};
static const swl_menuitem tswl_editar[] = {
    { "Copiar", 3, false },
    { "Colar",  4, false },
};
static const swl_menuitem tswl_config[] = {
    { "Preferencias...",  5, false },
};
static const swl_menu tswl_menus[] = {
    { "Arquivo",  tswl_arquivo,  2 },
    { "Editar",  tswl_editar,  2 },
    { "Configurar",  tswl_config,  1 },
};

static void execute_action(struct app *a, int id) {
    switch (id) {
    case TSWL_ACTION_LIMPAR: {
        if (a->pty_fd >=  0) {
            write(a->pty_fd, "\033[H\033[2J",  7);
        }
        tswl_term_scroll_view(a->term, -tswl_term_rows(a->term));
        a->need_redraw = true;
        break;
    }
    case TSWL_ACTION_SAIR: {
        a->running = false;
        break;
    }
    }
}

static void pointer_enter(void *data, struct wl_pointer *pointer,
        uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy) {
    struct app *a = data;
    (void)pointer;(void)serial;(void)surface;
    a->pointer_x = wl_fixed_to_int(sx);
    a->pointer_y = wl_fixed_to_int(sy);
    if (a->menubar) {
        if (swl_menubar_is_open(a->menubar)) {
            swl_menubar_pointer_motion(a->menubar, a->pointer_x, a->pointer_y);
            a->need_redraw = true;
        }
    }
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
        uint32_t serial, struct wl_surface *surface) {
    (void)data;(void)pointer;(void)serial;(void)surface;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
        uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    struct app *a = data;
    (void)pointer;(void)time;
    a->pointer_x = wl_fixed_to_int(sx);
    a->pointer_y = wl_fixed_to_int(sy);
    if (a->menubar) {
        swl_menubar_pointer_motion(a->menubar, a->pointer_x, a->pointer_y);
        if (swl_menubar_is_open(a->menubar)) a->need_redraw = true;
    }
    if (a->selecting && a->term) {
        int c, r;
        pixel_to_cell(a, a->pointer_x, a->pointer_y, &c, &r);
        tswl_term_set_selection(a->term, a->sel_anchor_c, a->sel_anchor_r, c, r);
        a->need_redraw = true;
    }
}

static void pointer_button(void *data, struct wl_pointer *pointer,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    struct app *a = data;
    (void)pointer;(void)serial;(void)time;
    bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    int by = a->pointer_y;

    /* menubar tem prioridade; menu nunca e arrasto: desliga selecao
     * travada (drag terminado sobre a menubar). Provado via log QEMU. */
    if (a->menubar && (swl_menubar_is_open(a->menubar) || by < TSWL_MENUBAR_H)) {
        a->selecting = false;
        int id = swl_menubar_pointer_button(a->menubar, a->pointer_x, by, pressed);
        if (id > 0) execute_action(a, id);
        a->need_redraw = true;
        return;
    }

    if (!a->term) return;
    int c, r;
    pixel_to_cell(a, a->pointer_x, a->pointer_y, &c, &r);

    if (button == BTN_LEFT) {
        if (pressed) {
            long now = now_ms();
            int same = (a->last_click_ms > 0
                && now - a->last_click_ms < 400
                && c == a->last_click_c && r == a->last_click_r);
            if (same)
                a->click_count++;
            else
                a->click_count = 1;
            a->last_click_ms = now;
            a->last_click_c = c;
            a->last_click_r = r;
            if (a->click_count >= 3) {
                /* triplo: linha inteira */
                tswl_term_select_line(a->term, r);
                a->selecting = false;
                a->click_count = 0;
                a->need_redraw = true;
            } else if (a->click_count == 2) {
                tswl_term_select_word(a->term, c, r);
                a->selecting = false;
                a->need_redraw = true;
            } else {
                a->selecting = true;
                a->sel_anchor_c = c;
                a->sel_anchor_r = r;
                tswl_term_set_selection(a->term, c, r, c, r);
                a->need_redraw = true;
            }
        } else {
            if (a->selecting) {
                a->selecting = false;
                if (a->sel_anchor_c == c && a->sel_anchor_r == r) {
                    tswl_term_clear_selection(a->term);
                    a->need_redraw = true;
                }
            }
        }
    } else if (button == BTN_RIGHT && pressed) {
        if (tswl_term_has_selection(a->term))
            selection_copy(a);
    } else if (button == BTN_MIDDLE && pressed) {
        clipboard_paste(a);
    }
}

static void pointer_axis(void *data, struct wl_pointer *p, uint32_t time,
        uint32_t axis, wl_fixed_t value) {
    struct app *a = data;
    (void)p; (void)time;
    if (!a->term) return;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
    double d = wl_fixed_to_double(value);
    /* Wayland: value > 0 = scroll "para baixo" (histórico mais recente).
     * scroll_offset maior = olhar para o passado. */
    int lines = 3;
    if (d > 0.0)
        tswl_term_scroll_view(a->term, -lines);
    else if (d < 0.0)
        tswl_term_scroll_view(a->term, lines);
    a->need_redraw = true;
}
static void pointer_frame(void *data, struct wl_pointer *p) {
    (void)data; (void)p;
}
static void pointer_axis_source(void *data, struct wl_pointer *p, uint32_t src) {
    (void)data; (void)p; (void)src;
}
static void pointer_axis_stop(void *data, struct wl_pointer *p, uint32_t time,
        uint32_t axis) {
    (void)data; (void)p; (void)time; (void)axis;
}
static void pointer_axis_discrete(void *data, struct wl_pointer *p,
        uint32_t axis, int32_t discrete) {
    /* Ignorado de propósito: o evento axis já cobre wheel e touchpad.
     * Tratar os dois dobraria o scroll em compositors que emitem ambos. */
    (void)data; (void)p; (void)axis; (void)discrete;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *a = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) {
        wl_keyboard_destroy(a->keyboard);
        a->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !a->pointer) {
        a->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(a->pointer, &pointer_listener, a);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && a->pointer) {
        wl_pointer_destroy(a->pointer);
        a->pointer = NULL;
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

    /* xkb_ctx ANTES do primeiro roundtrip: o seat pode entregar
     * wl_keyboard.keymap imediatamente no bind (capabilities), e o
     * listener chama xkb_keymap_new_from_string com o contexto.
     * Criar depois → null deref (R-02). */
    a.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!a.xkb_ctx) {
        fprintf(stderr, "tswl: falha ao criar xkb_context\n");
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
    a.menubar = swl_menubar_new(a.width, tswl_menus, 3);
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
                if (tswl_term_take_bell(a.term)) {
                    a.bell_until_ms = now_ms() + 120;
                    a.need_redraw = true;
                }
                {
                    char title[256];
                    if (tswl_term_take_title(a.term, title, sizeof(title))) {
                        xdg_toplevel_set_title(a.toplevel,
                            title[0] ? title : "TSWL");
                    }
                }
                {
                    char *clip = NULL;
                    size_t clen = 0;
                    if (tswl_term_take_clipboard(a.term, &clip, &clen)) {
                        free(a.clipboard);
                        a.clipboard = clip;
                        a.clipboard_len = clen;
                    }
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

        if (a.need_redraw && a.configured && has_any_buffer(&a)) {
            redraw(&a);
        }
    }

    /* limpeza */
    if (a.child_pid > 0) {
        kill(a.child_pid, SIGHUP);
        waitpid(a.child_pid, NULL, WNOHANG);
    }
    if (a.pty_fd >= 0) close(a.pty_fd);
    for (int i = 0; i < TSWL_BUF_COUNT; i++) {
        shm_buf_free_resources(&a.bufs[i]);
    }
    free(a.clipboard);
    if (a.keyboard) wl_keyboard_destroy(a.keyboard);
    if (a.pointer) wl_pointer_destroy(a.pointer);
    if (a.keymap) xkb_keymap_unref(a.keymap);
    if (a.xkb_state) xkb_state_unref(a.xkb_state);
    if (a.xkb_ctx) xkb_context_unref(a.xkb_ctx);
    tswl_term_free(a.term);
    tswl_render_free(a.render);
    swl_menubar_free(a.menubar);
    xdg_toplevel_destroy(a.toplevel);
    xdg_surface_destroy(a.xsurface);
    wl_surface_destroy(a.surface);
    wl_display_disconnect(a.display);
    return 0;
}
