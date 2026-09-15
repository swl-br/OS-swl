/*
 * main.c - cliente Wayland do swlsysinfo: xdg-shell + shm ARGB8888.
 * Desenha com cairo/pango (render.c) e copia a surface pro buffer shm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <cairo/cairo.h>
#include "render.h"
#include "monitor.h"
#include "xdg-shell-client-protocol.h"

#define TICK_MS 1000

struct client {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm;

    struct xdg_toplevel *toplevel;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct wl_buffer *buffer;
    struct wl_callback *frame_cb;
    int width, height;
    struct wl_surface *wl_surf;
    struct xdg_surface *xdg_surface;
    int running;
    int mapped;    int needs_redraw;    int last_tick;
    uint32_t serial;
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
    xkb_keysym_t keysym;
    char keybuf[64];
    struct swlsysinfo_snapshot snap, prev;
    struct swlsysinfo_render *render;
};

static void noop(void) {}

static int shm_create_fd(size_t size)
{
    char name[32];
    snprintf(name, sizeof(name), "swlsysinfo-shm-%d", getpid());
    int fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        shm_unlink(name);
        return -1;
    }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *wb)
{
    (void)data; (void)wb;
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_release,
};

static void shm_format(void *data, struct wl_shm *shm, uint32_t format)
{
    (void)data; (void)shm; (void)format;
}

static const struct wl_shm_listener shm_listener = {
    shm_format,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_listener = {
    xdg_wm_base_ping,
};

static void toplevel_configure(void *data, struct xdg_toplevel *tl, int32_t w, int32_t h, struct wl_array *states)
{
    struct client *c = data; (void)tl; (void)states;
    if (w > 0 && h > 0) { c->width = w; c->height = h; }
}

static void toplevel_close(void *data, struct xdg_toplevel *tl)
{
    struct client *c = data; (void)tl;
    c->running = 0;
}

static void toplevel_bounds(void *data, struct xdg_toplevel *tl, int32_t w, int32_t h)
{
    struct client *c = data; (void)tl;(void)w;(void)h;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    toplevel_configure,
    toplevel_close,
    toplevel_bounds,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
    struct client *c = data;
    xdg_surface_ack_configure(xs, serial);
    c->serial = serial;
    if (!c->mapped)
    {
        c->mapped = 1;
        wl_surface_commit(c->wl_surf);
    }
    c->needs_redraw =  1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure,
};

static void registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver)
{
    struct client *c = data;
    if (strcmp(iface, "wl_compositor") == 0 && ver >=  4)
        c->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (strcmp(iface, "wl_shm") == 0)
        c->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (strcmp(iface, "xdg_wm_base") == 0 && ver >=  1)
        c->wm = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
    else if (strcmp(iface, "wl_seat") == 0 && ver >=  4)
        c->seat = wl_registry_bind(reg, name, &wl_seat_interface, 4);
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    struct client *c = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !c->keyboard)
        c->keyboard = wl_seat_get_keyboard(seat);
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && c->keyboard)
    {
        wl_keyboard_release(c->keyboard);
        c->keyboard = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !c->pointer)
        c->pointer = wl_seat_get_pointer(seat);
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && c->pointer)
    {
        wl_pointer_release(c->pointer);
        c->pointer = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data; (void)seat; (void)name;
}

static const struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name,
};

static void keymap(void *data, struct wl_keyboard *kb, uint32_t format, int fd, uint32_t size)
{
    struct client *c = data; (void)kb;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
    char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { close(fd); return; }
    if (c->xkb_keymap) xkb_keymap_unref(c->xkb_keymap);
    if (c->xkb_state) xkb_state_unref(c->xkb_state);
    c->xkb_keymap = xkb_keymap_new_from_string(c->xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    munmap(map, size); close(fd);
    if (!c->xkb_keymap) return;
    c->xkb_state = xkb_state_new(c->xkb_keymap);
}

static void key_enter(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surf, struct wl_array *keys)
{
    (void)data; (void)kb; (void)serial;  (void)surf;(void)keys;
}

static void key_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surf)
{
    (void)data; (void)kb; (void)serial;  (void)surf;
}

static void key_evt(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    struct client *c = data; (void)kb; (void)serial; 
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    if (!c->xkb_state) return;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(c->xkb_state, key + 8);
    if (sym == XKB_KEY_Escape) { c->running =  0; return; }
    if (sym >= XKB_KEY_1 && sym <= XKB_KEY_9)
    {
        char ch = (char)(sym - XKB_KEY_1 +  1);
        snprintf(c->keybuf, sizeof(c->keybuf), "F%d", ch);
    }
}

static void key_mods(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    struct client *c = data;
    if (c->xkb_state) xkb_state_update_mask(c->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keymap,
    key_enter,
    key_leave,
    key_evt,
    key_mods,
};

static void destroy_buffer(struct client *c)
{
    if (c->buffer) wl_buffer_destroy(c->buffer);
    c->buffer = NULL;
}

static void make_buffer(struct client *c)
{
    if (!c->width || !c->height) return;
    int stride = c->width * 4;
    int size = stride * c->height;
    char name[48];
    snprintf(name, sizeof(name), "swlsysinfo-%d-%d", getpid(), c->width * c->height);
    int fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd <  0) return;
    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        shm_unlink(name);
        return;
    }
    void *pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pool_data == MAP_FAILED)
    {
        close(fd);
        shm_unlink(name);
        return;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(c->shm, fd, size);
    close(fd);
    if (!pool)
    {
        munmap(pool_data, size);
        shm_unlink(name);
        return;
    }
    c->buffer = wl_shm_pool_create_buffer(pool, 0, c->width, c->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!c->buffer)
    {
        munmap(pool_data, size);
        shm_unlink(name);
        return;
    }
    wl_buffer_add_listener(c->buffer, &buffer_listener, c);
    /* copia a surface cairo pro buffer */
    cairo_surface_t *surf = swlsysinfo_render_surface(c->render);
    unsigned char *src = cairo_image_surface_get_data(surf);
    cairo_format_t fmt = cairo_image_surface_get_format(surf);
    if (src && fmt == CAIRO_FORMAT_ARGB32)
    {
        int sstride = cairo_image_surface_get_stride(surf);
        int dst_stride = c->width * 4;
        unsigned char *dst = pool_data;
        for (int y =  0; y < c->height; y++)
            memcpy(dst + y * dst_stride, src + y * sstride, dst_stride);
    }
    munmap(pool_data, size);
    shm_unlink(name);
}

static void redraw(struct client *c)
{
    if (!c->width || !c->height) return;
    swlsysinfo_snapshot(&c->snap, &c->prev);
    swlsysinfo_render_draw(c->render, &c->snap);
    destroy_buffer(c);
    make_buffer(c);
    c->prev = c->snap;
    struct wl_surface *s = c->wl_surf;
    wl_surface_attach(s, c->buffer, 0, 0);
    wl_surface_damage_buffer(s, 0, 0, c->width, c->height);
    wl_surface_commit(s);
}
static void frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    struct client *c = data; (void)cb; 
    wl_callback_destroy(cb);
    c->frame_cb = NULL;
    if (!c->running) return;
    redraw(c);
}

static const struct wl_callback_listener frame_listener = {
    frame_done,
};

static void create_frame_cb(struct client *c)
{
    if (c->frame_cb) return;
    struct wl_surface *s = c->wl_surf;
    c->frame_cb = wl_surface_frame(s);
    wl_callback_add_listener(c->frame_cb, &frame_listener, c);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    struct client c;
    memset(&c, 0, sizeof(c));
    c.running = 1;
    c.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!c.xkb_ctx) return 1;
    c.display = wl_display_connect(NULL);
    if (!c.display) { fprintf(stderr, "swlsysinfo: nao ha Wayland\n"); return 1; }
    c.registry = wl_display_get_registry(c.display);
    wl_registry_add_listener(c.registry, &registry_listener, &c);
    wl_display_roundtrip(c.display);
    if (!c.compositor || !c.shm || !c.wm)
    {
        fprintf(stderr, "swlsysinfo: Wayland incompleto\n"); return 1;
    }
    c.render = swlsysinfo_render_new(1280, 720);
    if (!c.render) return 1;
    /* Tamanho inicial próprio: o compositor pode mandar configure 0x0 e
     * o primeiro quadro nunca sairia (redraw exige dims > 0). */
    c.width = 640;
    c.height = 400;
    struct wl_surface *surf = wl_compositor_create_surface(c.compositor);
    c.wl_surf = surf;
    c.xdg_surface = xdg_wm_base_get_xdg_surface(c.wm, surf);
    xdg_surface_add_listener(c.xdg_surface, &xdg_surface_listener, &c);
    c.toplevel = xdg_surface_get_toplevel(c.xdg_surface);
    xdg_toplevel_add_listener(c.toplevel, &toplevel_listener, &c);
    xdg_toplevel_set_title(c.toplevel, "SWL SysInfo — Sistema");
    xdg_toplevel_set_app_id(c.toplevel, "swlsysinfo");
    wl_surface_commit(surf);
    if (c.seat)
    {
        wl_seat_add_listener(c.seat, &seat_listener, &c);
    }
    while (c.running)
    {
        create_frame_cb(&c);
        wl_display_flush(c.display);
        if (wl_display_dispatch(c.display) < 0) break;
        /* Consome o redesenho pedido pelo configure: sem isso o primeiro
         * quadro nunca é desenhado (frame só vem pra quem já tem buffer). */
        if (c.needs_redraw && c.width > 0 && c.height > 0) {
            c.needs_redraw = 0;
            redraw(&c);
        }
    }
    swlsysinfo_render_free(c.render);
    wl_display_disconnect(c.display);
    xkb_context_unref(c.xkb_ctx);
    return 0;
}
