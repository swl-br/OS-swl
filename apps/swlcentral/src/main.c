#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <math.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>
#include "xdg-shell-client-protocol.h"
#include "central.h"

#define INIT_W 760
#define INIT_H 620

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int create_shm_file(size_t size) {
    char name[] = "/swlcentral-XXXXXX";
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return -1;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *b) { (void)data; (void)b; }
static const struct wl_buffer_listener buffer_listener = { .release = buffer_release };

static bool recreate_buffer(cc_app_t *a) {
    if (a->buffer) { wl_buffer_destroy(a->buffer); a->buffer = NULL; }
    if (a->buffer_data) { munmap(a->buffer_data, a->buffer_size); a->buffer_data = NULL; }
    if (a->csurf) { cairo_surface_destroy(a->csurf); a->csurf = NULL; }
    if (a->cr) { cairo_destroy(a->cr); a->cr = NULL; }
    size_t stride = (size_t)a->width * 4;
    size_t size = stride * a->height;
    int fd = create_shm_file(size);
    if (fd < 0) return false;
    a->buffer_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (a->buffer_data == MAP_FAILED) { a->buffer_data = NULL; close(fd); return false; }
    a->buffer_size = size;
    a->csurf = cairo_image_surface_create_for_data(
        a->buffer_data, CAIRO_FORMAT_ARGB32, a->width, a->height, (int)stride);
    a->cr = cairo_create(a->csurf);
    struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, (int)size);
    a->buffer = wl_shm_pool_create_buffer(pool, 0, a->width, a->height,
                                          (int)stride, WL_SHM_FORMAT_ARGB8888);
    wl_buffer_add_listener(a->buffer, &buffer_listener, a);
    wl_shm_pool_destroy(pool);
    close(fd);
    if (a->menubar) swl_menubar_resize(a->menubar, a->width);
    return true;
}

static void redraw(cc_app_t *a) {
    cc_draw_all(a);
    cairo_surface_flush(a->csurf);
    wl_surface_attach(a->surface, a->buffer, 0, 0);
    wl_surface_damage_buffer(a->surface, 0, 0, a->width, a->height);
    wl_surface_commit(a->surface);
    a->need_redraw = false;
}

static void clamp_scroll(cc_app_t *a) {
    double max = a->content_h - a->view_h;
    if (max < 0) max = 0;
    if (a->scroll < 0) a->scroll = 0;
    if (a->scroll > max) a->scroll = max;
}

static void xsurface_configure(void *d, struct xdg_surface *xs, uint32_t s) {
    cc_app_t *a = d;
    xdg_surface_ack_configure(xs, s);
    a->configured = true;
    if (!a->buffer && !recreate_buffer(a)) { a->running = false; return; }
    a->need_redraw = true;
}
static const struct xdg_surface_listener xsurface_listener = { .configure = xsurface_configure };

static void toplevel_configure(void *d, struct xdg_toplevel *tl, int32_t w, int32_t h, struct wl_array *st) {
    cc_app_t *a = d; (void)tl; (void)st;
    if (w == 0 || h == 0) return;
    if (w < CC_MIN_W) w = CC_MIN_W;
    if (h < CC_MIN_H) h = CC_MIN_H;
    if (w != a->width || h != a->height) {
        a->width = w; a->height = h;
        if (a->configured) { recreate_buffer(a); clamp_scroll(a); a->need_redraw = true; }
    }
}
static void toplevel_close(void *d, struct xdg_toplevel *tl) { (void)tl; ((cc_app_t *)d)->running = false; }
static void toplevel_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h) { (void)d; (void)t; (void)w; (void)h; }
static void toplevel_caps(void *d, struct xdg_toplevel *t, struct wl_array *c) { (void)d; (void)t; (void)c; }
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure, .close = toplevel_close,
    .configure_bounds = toplevel_bounds, .wm_capabilities = toplevel_caps,
};

static void kb_keymap(void *d, struct wl_keyboard *k, uint32_t f, int32_t fd, uint32_t sz) {
    cc_app_t *a = d; (void)k;
    if (f != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
    char *map = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { close(fd); return; }
    struct xkb_keymap *km = xkb_keymap_new_from_string(a->xkb_ctx, map,
        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, sz); close(fd);
    if (!km) return;
    struct xkb_state *st = xkb_state_new(km);
    if (!st) { xkb_keymap_unref(km); return; }
    if (a->keymap) xkb_keymap_unref(a->keymap);
    if (a->xkb_state) xkb_state_unref(a->xkb_state);
    a->keymap = km; a->xkb_state = st;
}
static void kb_enter(void *d, struct wl_keyboard *k, uint32_t s, struct wl_surface *sf, struct wl_array *ky) {
    (void)d; (void)k; (void)s; (void)sf; (void)ky; }
static void kb_leave(void *d, struct wl_keyboard *k, uint32_t s, struct wl_surface *sf) {
    (void)d; (void)k; (void)s; (void)sf; }
static void kb_mods(void *d, struct wl_keyboard *k, uint32_t s, uint32_t dep, uint32_t lat, uint32_t lok, uint32_t grp) {
    cc_app_t *a = d; (void)k; (void)s;
    if (a->xkb_state) xkb_state_update_mask(a->xkb_state, dep, lat, lok, 0, 0, grp);
}
static void kb_repeat(void *d, struct wl_keyboard *k, int32_t r, int32_t dl) { (void)d; (void)k; (void)r; (void)dl; }

static void search_type(cc_app_t *a, uint32_t keycode, xkb_keysym_t sym) {
    if (sym == XKB_KEY_BackSpace) {
        size_t n = strlen(a->search);
        while (n > 0 && (a->search[n - 1] & 0xC0) == 0x80) n--;
        if (n > 0) n--;
        a->search[n] = 0;
        cc_refresh_search(a);
        a->need_redraw = true;
        return;
    }
    if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
        if (a->search_nhits > 0) {
            a->search[0] = 0;
            a->search_nhits = 0;
            a->search_focused = false;
            a->need_redraw = true;
        }
        return;
    }
    char utf8[16];
    int n = xkb_state_key_get_utf8(a->xkb_state, keycode, utf8, sizeof(utf8) - 1);
    if (n > 0) {
        utf8[n] = 0;
        size_t cur = strlen(a->search);
        if (cur + (size_t)n < sizeof(a->search) - 1 && utf8[0] >= 32) {
            memcpy(a->search + cur, utf8, (size_t)n + 1);
            cc_refresh_search(a);
            a->need_redraw = true;
        }
    }
}

static void kb_key(void *d, struct wl_keyboard *k, uint32_t s, uint32_t t, uint32_t key, uint32_t state) {
    cc_app_t *a = d; (void)k; (void)s; (void)t;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !a->xkb_state) return;
    uint32_t keycode = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(a->xkb_state, keycode);

    if (sym == XKB_KEY_Escape) {
        if (a->dialog.open) { a->dialog.open = false; a->need_redraw = true; }
        else if (a->drop.open) { a->drop.open = false; a->need_redraw = true; }
        else if (a->search_focused) { a->search_focused = false; a->need_redraw = true; }
        else if (swl_menubar_is_expanded(a->menubar)) { swl_menubar_pointer_button(a->menubar, 10, 10, true); a->need_redraw = true; }
        else a->running = false;
        return;
    }
    if (a->dialog.open && a->dialog.has_field && a->dialog.field.focused) {
        if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
            cc_dispatch(a, a->dialog.action_ok, a->dialog.action_arg);
            a->dialog.open = false;
            a->need_redraw = true;
            return;
        }
        if (cc_field_key(&a->dialog.field, a->xkb_state, keycode, sym)) a->need_redraw = true;
        return;
    }
    if (a->search_focused) {
        search_type(a, keycode, sym);
        return;
    }
    xkb_mod_mask_t ctrl = xkb_state_mod_name_is_active(a->xkb_state, "Control", XKB_STATE_MODS_EFFECTIVE);
    if (ctrl && sym == XKB_KEY_q) a->running = false;
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = kb_keymap, .enter = kb_enter, .leave = kb_leave, .key = kb_key,
    .modifiers = kb_mods, .repeat_info = kb_repeat,
};

static void ptr_enter(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p; (void)s; (void)sf; (void)x; (void)y; }
static void ptr_leave(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf) {
    (void)d; (void)p; (void)s; (void)sf; }

static void ptr_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t sx, wl_fixed_t sy) {
    cc_app_t *a = d; (void)p; (void)t;
    int px = wl_fixed_to_int(sx), py = wl_fixed_to_int(sy);
    a->px = px; a->py = py;
    if (a->scrollbar_drag) {
        int rx, ry, rw, bot, sx2, sy2, sw2;
        cc_contentgeom(a, &sx2, &sy2, &sw2, &rx, &ry, &rw, &bot);
        double view = a->view_h, total = a->content_h;
        if (total > view) {
            double h = (bot - ry) * view / total;
            if (h < 30) h = 30;
            double frac = (py - ry - h / 2) / ((bot - ry) - h);
            if (frac < 0) frac = 0;
            if (frac > 1) frac = 1;
            a->scroll = frac * (total - view);
            a->need_redraw = true;
        }
        return;
    }
    if (a->dialog.open) return;
    swl_menubar_pointer_motion(a->menubar, px, py);
    a->need_redraw = true;
    if (a->drop.open) return;
    int cx, cy, cw, rx, ry, rw, bot;
    cc_contentgeom(a, &cx, &cy, &cw, &rx, &ry, &rw, &bot);
    if (py >= ry && py < bot && px >= rx) {
        cc_page_motion(a, px, py + (int)a->scroll);
    }
}

static void content_press(cc_app_t *a, int px, int py) {
    int sx, sy, sw, rx, ry, rw, bot;
    cc_contentgeom(a, &sx, &sy, &sw, &rx, &ry, &rw, &bot);
    if (px >= a->width - 14 && py >= ry && py < bot) {
        a->scrollbar_drag = true;
        a->need_redraw = true;
        return;
    }
    if (py >= ry && py < bot && px >= rx && px < a->width - 14) {
        cc_page_press(a, px, py + (int)a->scroll);
    }
}

static void ptr_button(void *d, struct wl_pointer *p, uint32_t s, uint32_t t, uint32_t btn, uint32_t state) {
    cc_app_t *a = d; (void)p; (void)s; (void)t;
    if (btn != BTN_LEFT) return;
    int px = a->px, py = a->py;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (a->dialog.open) {
            double ok_x, ok_y, ok_w, ok_h, c_x, c_y, c_w, c_h, p_x, p_y, p_w, p_h;
            swl_modal_draw(a->cr, &a->theme, a->width, a->height, a->dialog.title, a->dialog.body,
                           a->dialog.ok_label, SWL_BTN_PRIMARY, "Cancelar",
                           &ok_x, &ok_y, &ok_w, &ok_h, &c_x, &c_y, &c_w, &c_h, &p_x, &p_y, &p_w, &p_h);
            if (a->dialog.has_field && swl_rect_hit(p_x + 20, ok_y - 44, p_w - 40, 32, px, py)) {
                a->dialog.field.focused = true;
                a->need_redraw = true;
                return;
            }
            if (swl_rect_hit(ok_x, ok_y, ok_w, ok_h, px, py)) {
                cc_dispatch(a, a->dialog.action_ok, a->dialog.action_arg);
                a->dialog.open = false; a->need_redraw = true; return;
            }
            if (c_w > 0 && swl_rect_hit(c_x, c_y, c_w, c_h, px, py)) {
                a->dialog.open = false; a->need_redraw = true; return;
            }
            return;
        }
        if (a->drop.open) {
            int hit = swl_dropdown_list_hit(a->drop.x, a->drop.y + a->drop.h, a->drop.w, a->drop.nopts, px, py);
            if (hit >= 0) {
                *a->drop.sel = hit;
                cc_dispatch(a, a->drop.action, hit);
            }
            a->drop.open = false;
            a->need_redraw = true;
            return;
        }
        if (a->search_focused && a->search_nhits > 0) {
            int mx, my, mw, mh; swl_menubar_search_rect(a->menubar, &mx, &my, &mw, &mh);
            int hit = swl_menubar_search_hit_at(a->menubar, px, py);
            if (hit >= 0) {
                a->sel = a->search_cats[hit];
                a->search[0] = 0; a->search_nhits = 0; a->search_focused = false;
                a->need_redraw = true;
                return;
            }
        }
        int id = swl_menubar_pointer_button(a->menubar, px, py, true);
        if (id) { a->need_redraw = true; return; }
        if (py < SWL_MENUBAR_BAR_H && swl_menubar_is_expanded(a->menubar)) {
            int mx, my, mw, mh; swl_menubar_search_rect(a->menubar, &mx, &my, &mw, &mh);
            a->search_focused = swl_rect_hit(mx, my, mw, mh, px, py);
            a->need_redraw = true;
            if (a->search_focused) return;
        }
        if (py < SWL_MENUBAR_BAR_H) { a->need_redraw = true; return; }
        content_press(a, px, py);
    } else {
        a->scrollbar_drag = false;
        cc_page_release(a);
    }
}

static void ptr_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t ax, wl_fixed_t v) {
    cc_app_t *a = d; (void)p; (void)t;
    if (ax != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
    double dv = wl_fixed_to_double(v);
    if (dv == 0) return;
    a->scroll += dv > 0 ? 48 : -48;
    clamp_scroll(a);
    a->need_redraw = true;
}
static void ptr_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void ptr_axis_src(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void ptr_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t ax) { (void)d; (void)p; (void)t; (void)ax; }
static void ptr_axis_disc(void *d, struct wl_pointer *p, uint32_t ax, int32_t disc) {
    cc_app_t *a = d; (void)p; (void)ax;
    a->scroll += disc > 0 ? 48 : -48;
    clamp_scroll(a);
    a->need_redraw = true;
}
static const struct wl_pointer_listener pointer_listener = {
    .enter = ptr_enter, .leave = ptr_leave, .motion = ptr_motion, .button = ptr_button,
    .axis = ptr_axis, .frame = ptr_frame, .axis_source = ptr_axis_src,
    .axis_stop = ptr_axis_stop, .axis_discrete = ptr_axis_disc,
};

static void seat_caps(void *d, struct wl_seat *s, uint32_t caps) {
    cc_app_t *a = d;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
        a->keyboard = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && a->keyboard) {
        wl_keyboard_destroy(a->keyboard); a->keyboard = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !a->pointer) {
        a->pointer = wl_seat_get_pointer(s);
        wl_pointer_add_listener(a->pointer, &pointer_listener, a);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && a->pointer) {
        wl_pointer_destroy(a->pointer); a->pointer = NULL;
    }
}
static void seat_name(void *d, struct wl_seat *s, const char *n) { (void)d; (void)s; (void)n; }
static const struct wl_seat_listener seat_listener = { .capabilities = seat_caps, .name = seat_name };

static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t s) { (void)d; xdg_wm_base_pong(b, s); }
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };

static void reg_global(void *d, struct wl_registry *r, uint32_t n, const char *iface, uint32_t v) {
    cc_app_t *a = d;
    if (strcmp(iface, wl_compositor_interface.name) == 0)
        a->compositor = wl_registry_bind(r, n, &wl_compositor_interface, v < 4 ? v : 4);
    else if (strcmp(iface, wl_shm_interface.name) == 0)
        a->shm = wl_registry_bind(r, n, &wl_shm_interface, 1);
    else if (strcmp(iface, xdg_wm_base_interface.name) == 0) {
        a->wm_base = wl_registry_bind(r, n, &xdg_wm_base_interface, v < 3 ? v : 3);
        xdg_wm_base_add_listener(a->wm_base, &wm_listener, a);
    } else if (strcmp(iface, wl_seat_interface.name) == 0) {
        a->seat = wl_registry_bind(r, n, &wl_seat_interface, v < 5 ? v : 5);
        wl_seat_add_listener(a->seat, &seat_listener, a);
    }
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener registry_listener = { .global = reg_global, .global_remove = reg_remove };

static const swl_menuitem m_arquivo[] = {
    {"Backup agora", CC_ACT_BACKUP_NOW, true},
    {NULL, 0, false},
    {"Sair", -1, true},
};
static const swl_menuitem m_editar[] = {
    {"Restaurar padrões", CC_ACT_RESET_DEFAULTS, true},
};
static const swl_menuitem m_ajuda[] = {
    {"Sobre", -2, true},
};
static const swl_menu MENUS[] = {
    {"Arquivo", m_arquivo, 3},
    {"Editar", m_editar, 1},
    {"Ajuda", m_ajuda, 1},
};

int main(void) {
    cc_app_t a = {0};
    a.width = INIT_W; a.height = INIT_H;
    a.running = true;
    a.sel = CC_CAT_REDE;
    a.advanced = true;

    cc_api_conf_load(&a);
    a.theme = swl_theme_load();

    a.display = wl_display_connect(NULL);
    if (!a.display) { fprintf(stderr, "swlcentral: sem Wayland\n"); return 1; }
    a.registry = wl_display_get_registry(a.display);
    wl_registry_add_listener(a.registry, &registry_listener, &a);
    wl_display_roundtrip(a.display);
    if (!a.compositor || !a.shm || !a.wm_base) {
        fprintf(stderr, "swlcentral: compositor/shm/xdg indisponivel\n"); return 1;
    }
    a.surface = wl_compositor_create_surface(a.compositor);
    a.xsurface = xdg_wm_base_get_xdg_surface(a.wm_base, a.surface);
    xdg_surface_add_listener(a.xsurface, &xsurface_listener, &a);
    a.toplevel = xdg_surface_get_toplevel(a.xsurface);
    xdg_toplevel_add_listener(a.toplevel, &toplevel_listener, &a);
    xdg_toplevel_set_title(a.toplevel, "Central de Ajustes");
    xdg_toplevel_set_app_id(a.toplevel, "swlcentral");
    wl_surface_commit(a.surface);
    wl_display_roundtrip(a.display);
    a.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    a.menubar = swl_menubar_new(a.width, MENUS, 3);

    int wl_fd = wl_display_get_fd(a.display);
    long last = now_ms();
    while (a.running) {
        if (wl_display_dispatch_pending(a.display) == -1) break;
        wl_display_flush(a.display);

        int timeout = 30;
        long t = now_ms();
        int dt = (int)(t - last);
        last = t;
        if (swl_menubar_animating(a.menubar)) {
            swl_menubar_tick(a.menubar, dt);
            a.need_redraw = true;
            timeout = 8;
        }
        struct pollfd pfd = { wl_fd, POLLIN, 0 };
        if (poll(&pfd, 1, timeout) > 0 && (pfd.revents & POLLIN)) {
            if (wl_display_dispatch(a.display) == -1) break;
        }
        if (a.need_redraw && a.configured && a.buffer) redraw(&a);
    }
    swl_menubar_free(a.menubar);
    return 0;
}
