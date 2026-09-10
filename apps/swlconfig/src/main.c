/*
 * main.c — swlconfig, app de Configurações nativo do SWL OS.
 *
 * Cliente Wayland puro (mesma arquitetura do TSWL: xdg-shell direto,
 * shm buffers ARGB8888, xkbcommon pro teclado, sem toolkit). Casca com
 * seções navegáveis por teclado (Up/Down); cada seção ganha função por
 * etapa — hoje só "Sobre" tem dado real (sysinfo.c), o resto mostra
 * "ainda não implementado" honestamente, sem fingir que está pronto
 * (ver README seção 8 / AFAZERES S1).
 *
 * Atalhos: Up/Down navega entre seções, Ctrl+Q sai.
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "xdg-shell-client-protocol.h"
#include "sysinfo.h"
#include "menubar.h"

#define INIT_WIDTH  760
#define INIT_HEIGHT 480
#define SIDEBAR_WIDTH 220
#define ROW_HEIGHT 34
#define FONT_DESC "Sans 11"

/* Paleta SWL OS (mesmos valores de swl-ui/include/theme.h e dos demais
 * apps — TSWL e SWLPad duplicam a mesma coisa; ver IDEIAS.md sobre
 * eventualmente centralizar isso quando o padrão de app for definido). */
#define COL_BG_R     0.043
#define COL_BG_G     0.055
#define COL_BG_B     0.078
#define COL_SIDEBAR_R 0.055
#define COL_SIDEBAR_G 0.071
#define COL_SIDEBAR_B 0.098
#define COL_TEXT_R   0.831
#define COL_TEXT_G   0.871
#define COL_TEXT_B   0.902
#define COL_DIM_R    0.451
#define COL_DIM_G    0.498
#define COL_DIM_B    0.561
#define COL_CYAN_R   0.420
#define COL_CYAN_G   0.820
#define COL_CYAN_B   0.800

static const char *SECTIONS[] = {
	"Sobre",
	"Wi-Fi / Rede",
	"Bluetooth",
	"Personalizar",
	"Teclado e mouse",
	"Display",
	"Painel",
	"Notificações",
	"Acessibilidade",
	"Apps",
	"Idiomas",
	"Disco",
};
#define N_SECTIONS ((int)(sizeof(SECTIONS) / sizeof(SECTIONS[0])))

/* Ações do menu — ids escolhidos por este app, o swlappkit só devolve
 * o id de volta quando um item é ativado. */
#define MENU_ACTION_SAIR  1
#define MENU_ACTION_SOBRE 2

static const swl_menuitem ARQUIVO_ITEMS[] = {
	{ "Sair", MENU_ACTION_SAIR, true },
};
static const swl_menuitem AJUDA_ITEMS[] = {
	{ "Sobre o SWL OS", MENU_ACTION_SOBRE, true },
};
static const swl_menu MENUS[] = {
	{ "Arquivo", ARQUIVO_ITEMS, 1 },
	{ "Ajuda", AJUDA_ITEMS, 1 },
};
#define N_MENUS ((int)(sizeof(MENUS) / sizeof(MENUS[0])))

typedef struct {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	int width, height, stride;
	bool busy;
} shm_buf_t;

typedef struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *wm_base;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;

	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	int width, height;
	int pending_width, pending_height;
	bool configured;
	bool running;

	shm_buf_t bufs[2];

	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;

	int selected_section;
	PangoFontDescription *font_desc;
	swl_menubar *menubar;
} app_t;

/* ---------------------------------------------------------------------- */
/* Buffer (memfd + wl_shm) — mesmo padrão do TSWL/SWLPad                  */
/* ---------------------------------------------------------------------- */

static int create_shm_fd(size_t size) {
	int fd = memfd_create("swlconfig-buffer", MFD_CLOEXEC);
	if (fd < 0) return -1;
	if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
	return fd;
}

static void destroy_shm_buffer(shm_buf_t *buf) {
	if (buf->wl_buffer) wl_buffer_destroy(buf->wl_buffer);
	if (buf->data && buf->data != MAP_FAILED) munmap(buf->data, buf->size);
	memset(buf, 0, sizeof(*buf));
}

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	(void)wl_buffer;
	shm_buf_t *buf = data;
	buf->busy = false;
}
static const struct wl_buffer_listener buffer_listener = { .release = buffer_handle_release };

static bool init_shm_buffer(app_t *app, shm_buf_t *buf, int width, int height) {
	int stride = width * 4;
	size_t size = (size_t)stride * (size_t)height;
	int fd = create_shm_fd(size);
	if (fd < 0) return false;
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) { close(fd); return false; }

	struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, (int32_t)size);
	struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	buf->wl_buffer = wl_buffer;
	buf->data = data;
	buf->size = size;
	buf->width = width;
	buf->height = height;
	buf->stride = stride;
	buf->busy = false;
	wl_buffer_add_listener(wl_buffer, &buffer_listener, buf);
	return true;
}

static shm_buf_t *get_free_buffer(app_t *app, int width, int height) {
	for (int i = 0; i < 2; i++) {
		shm_buf_t *b = &app->bufs[i];
		if (b->wl_buffer && !b->busy && b->width == width && b->height == height) return b;
	}
	for (int i = 0; i < 2; i++) {
		shm_buf_t *b = &app->bufs[i];
		if (!b->busy) {
			if (b->wl_buffer) destroy_shm_buffer(b);
			if (init_shm_buffer(app, b, width, height)) return b;
			return NULL;
		}
	}
	return NULL;
}

/* ---------------------------------------------------------------------- */
/* Desenho                                                                 */
/* ---------------------------------------------------------------------- */

static void draw_about_section(cairo_t *cr, PangoLayout *layout, int x, int y) {
	swlconfig_sysinfo_t info;
	swlconfig_sysinfo_collect(&info);

	char lines[10][512];
	int n = 0;

	snprintf(lines[n++], sizeof(lines[0]), "SWL OS");
	snprintf(lines[n++], sizeof(lines[0]), " ");
	snprintf(lines[n++], sizeof(lines[0]), "Kernel: %s",
	         info.kernel_release[0] ? info.kernel_release : "desconhecido");
	snprintf(lines[n++], sizeof(lines[0]), "Hostname: %s",
	         info.hostname[0] ? info.hostname : "desconhecido");

	if (info.uptime_seconds >= 0) {
		long h = info.uptime_seconds / 3600;
		long m = (info.uptime_seconds % 3600) / 60;
		snprintf(lines[n++], sizeof(lines[0]), "Tempo ligado: %ldh %ldmin", h, m);
	} else {
		snprintf(lines[n++], sizeof(lines[0]), "Tempo ligado: desconhecido");
	}

	if (info.mem_total_kb > 0) {
		double total_mb = info.mem_total_kb / 1024.0;
		double avail_mb = info.mem_available_kb > 0 ? info.mem_available_kb / 1024.0 : 0;
		snprintf(lines[n++], sizeof(lines[0]), "Memória: %.0f MB disponível de %.0f MB",
		         avail_mb, total_mb);
	} else {
		snprintf(lines[n++], sizeof(lines[0]), "Memória: desconhecida");
	}

	if (info.disk_total_gb > 0) {
		snprintf(lines[n++], sizeof(lines[0]), "Disco (/): %.1f GB livres de %.1f GB",
		         info.disk_free_gb, info.disk_total_gb);
	} else {
		snprintf(lines[n++], sizeof(lines[0]), "Disco: desconhecido");
	}

	int cur_y = y;
	for (int i = 0; i < n; i++) {
		pango_layout_set_text(layout, lines[i], -1);
		cairo_move_to(cr, x, cur_y);
		if (i == 0) {
			cairo_set_source_rgb(cr, COL_CYAN_R, COL_CYAN_G, COL_CYAN_B);
		} else {
			cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
		}
		pango_cairo_show_layout(cr, layout);
		int w, h;
		pango_layout_get_pixel_size(layout, &w, &h);
		(void)w;
		cur_y += h + 8;
	}
}

static void draw_placeholder_section(cairo_t *cr, PangoLayout *layout, int x, int y,
                                      const char *section_name) {
	char title[300];
	snprintf(title, sizeof(title), "%s", section_name);
	pango_layout_set_text(layout, title, -1);
	cairo_move_to(cr, x, y);
	cairo_set_source_rgb(cr, COL_CYAN_R, COL_CYAN_G, COL_CYAN_B);
	pango_cairo_show_layout(cr, layout);
	int w, h;
	pango_layout_get_pixel_size(layout, &w, &h);
	(void)w;

	pango_layout_set_text(layout, "Ainda não implementado nesta versão.", -1);
	cairo_move_to(cr, x, y + h + 12);
	cairo_set_source_rgb(cr, COL_DIM_R, COL_DIM_G, COL_DIM_B);
	pango_cairo_show_layout(cr, layout);
}

static void do_redraw(app_t *app) {
	if (!app->configured) return;

	shm_buf_t *buf = get_free_buffer(app, app->width, app->height);
	while (!buf) {
		if (wl_display_dispatch(app->display) == -1) return;
		buf = get_free_buffer(app, app->width, app->height);
	}

	cairo_surface_t *surf = cairo_image_surface_create_for_data(
		buf->data, CAIRO_FORMAT_ARGB32, buf->width, buf->height, buf->stride);
	cairo_t *cr = cairo_create(surf);

	/* fundo geral */
	cairo_set_source_rgb(cr, COL_BG_R, COL_BG_G, COL_BG_B);
	cairo_paint(cr);

	/* sidebar */
	cairo_set_source_rgb(cr, COL_SIDEBAR_R, COL_SIDEBAR_G, COL_SIDEBAR_B);
	cairo_rectangle(cr, 0, 0, SIDEBAR_WIDTH, buf->height);
	cairo_fill(cr);

	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(layout, app->font_desc);

	int top_offset = SWL_MENUBAR_BAR_H;

	for (int i = 0; i < N_SECTIONS; i++) {
		int row_y = top_offset + i * ROW_HEIGHT;
		if (i == app->selected_section) {
			cairo_set_source_rgba(cr, COL_CYAN_R, COL_CYAN_G, COL_CYAN_B, 0.18);
			cairo_rectangle(cr, 0, row_y, SIDEBAR_WIDTH, ROW_HEIGHT);
			cairo_fill(cr);
		}
		pango_layout_set_text(layout, SECTIONS[i], -1);
		cairo_move_to(cr, 16, row_y + 8);
		if (i == app->selected_section) {
			cairo_set_source_rgb(cr, COL_CYAN_R, COL_CYAN_G, COL_CYAN_B);
		} else {
			cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
		}
		pango_cairo_show_layout(cr, layout);
	}

	/* painel de conteúdo */
	int content_x = SIDEBAR_WIDTH + 24;
	int content_y = top_offset + 24;
	if (app->selected_section == 0) {
		draw_about_section(cr, layout, content_x, content_y);
	} else {
		draw_placeholder_section(cr, layout, content_x, content_y,
		                          SECTIONS[app->selected_section]);
	}

	g_object_unref(layout);

	/* barra de menu por cima de tudo (mesmo padrão visual do TSWL/SWLPad) */
	swl_menubar_draw(app->menubar, cr, buf->width);

	cairo_surface_flush(surf);
	cairo_destroy(cr);
	cairo_surface_destroy(surf);

	buf->busy = true;
	wl_surface_attach(app->surface, buf->wl_buffer, 0, 0);
	wl_surface_damage_buffer(app->surface, 0, 0, buf->width, buf->height);
	wl_surface_commit(app->surface);
	wl_display_flush(app->display);
}

/* ---------------------------------------------------------------------- */
/* xdg_wm_base / xdg_surface / xdg_toplevel                               */
/* ---------------------------------------------------------------------- */

static void wm_base_handle_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
	(void)data;
	xdg_wm_base_pong(wm_base, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = { .ping = wm_base_handle_ping };

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface,
                                          uint32_t serial) {
	app_t *app = data;
	xdg_surface_ack_configure(xdg_surface, serial);
	app->width = app->pending_width > 0 ? app->pending_width : app->width;
	app->height = app->pending_height > 0 ? app->pending_height : app->height;
	app->configured = true;
	if (app->menubar) {
		swl_menubar_resize(app->menubar, app->width);
	}
	do_redraw(app);
}
static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *toplevel,
                                           int32_t width, int32_t height, struct wl_array *states) {
	(void)toplevel; (void)states;
	app_t *app = data;
	app->pending_width = width;
	app->pending_height = height;
}
static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *toplevel) {
	(void)toplevel;
	app_t *app = data;
	app->running = false;
}
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

/* ---------------------------------------------------------------------- */
/* Teclado                                                                 */
/* ---------------------------------------------------------------------- */

static void keyboard_handle_keymap(void *data, struct wl_keyboard *kb, uint32_t format,
                                    int fd, uint32_t size) {
	(void)kb;
	app_t *app = data;
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
	char *map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_str == MAP_FAILED) { close(fd); return; }
	struct xkb_keymap *keymap = xkb_keymap_new_from_string(
		app->xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_str, size);
	close(fd);
	if (!keymap) return;
	if (app->xkb_state) xkb_state_unref(app->xkb_state);
	if (app->xkb_keymap) xkb_keymap_unref(app->xkb_keymap);
	app->xkb_keymap = keymap;
	app->xkb_state = xkb_state_new(keymap);
}
static void keyboard_handle_enter(void *d, struct wl_keyboard *k, uint32_t s,
                                   struct wl_surface *su, struct wl_array *keys) {
	(void)d; (void)k; (void)s; (void)su; (void)keys;
}
static void keyboard_handle_leave(void *d, struct wl_keyboard *k, uint32_t s,
                                   struct wl_surface *su) {
	(void)d; (void)k; (void)s; (void)su;
}
static void keyboard_handle_key(void *data, struct wl_keyboard *kb, uint32_t serial,
                                 uint32_t time, uint32_t key, uint32_t state) {
	(void)kb; (void)serial; (void)time;
	app_t *app = data;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->xkb_state) return;

	xkb_keycode_t keycode = key + 8;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, keycode);
	bool ctrl = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_CTRL,
	                                          XKB_STATE_MODS_EFFECTIVE) > 0;

	/* menu aberto: Left/Right/Up/Down/Enter/Esc pertencem a ele primeiro */
	if (swl_menubar_is_open(app->menubar)) {
		int action = 0;
		switch (sym) {
		case XKB_KEY_Left:   action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_LEFT);  break;
		case XKB_KEY_Right:  action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_RIGHT); break;
		case XKB_KEY_Up:     action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_UP);    break;
		case XKB_KEY_Down:   action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_DOWN);  break;
		case XKB_KEY_Return: action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_ENTER); break;
		case XKB_KEY_Escape: action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_ESC);   break;
		case XKB_KEY_Alt_L:
		case XKB_KEY_Alt_R:
			action = swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_ALT);
			break;
		default:
			break;
		}
		if (action == MENU_ACTION_SAIR) {
			app->running = false;
			return;
		}
		if (action == MENU_ACTION_SOBRE) {
			app->selected_section = 0;
		}
		do_redraw(app);
		return;
	}

	if (sym == XKB_KEY_Alt_L || sym == XKB_KEY_Alt_R) {
		swl_menubar_key(app->menubar, SWL_MENUBAR_KEY_ALT);
		do_redraw(app);
		return;
	}

	if (ctrl && (sym == XKB_KEY_q || sym == XKB_KEY_Q)) {
		app->running = false;
		return;
	}
	if (sym == XKB_KEY_Up) {
		if (app->selected_section > 0) app->selected_section--;
		do_redraw(app);
		return;
	}
	if (sym == XKB_KEY_Down) {
		if (app->selected_section < N_SECTIONS - 1) app->selected_section++;
		do_redraw(app);
		return;
	}
}
static void keyboard_handle_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
                                       uint32_t dep, uint32_t lat, uint32_t lock, uint32_t group) {
	(void)kb; (void)serial;
	app_t *app = data;
	if (app->xkb_state) xkb_state_update_mask(app->xkb_state, dep, lat, lock, 0, 0, group);
}
static void keyboard_handle_repeat_info(void *d, struct wl_keyboard *k, int32_t r, int32_t de) {
	(void)d; (void)k; (void)r; (void)de;
}
static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_handle_keymap,
	.enter = keyboard_handle_enter,
	.leave = keyboard_handle_leave,
	.key = keyboard_handle_key,
	.modifiers = keyboard_handle_modifiers,
	.repeat_info = keyboard_handle_repeat_info,
};

/* ---------------------------------------------------------------------- */
/* Seat / registry                                                        */
/* ---------------------------------------------------------------------- */

static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
	app_t *app = data;
	bool has_kb = caps & WL_SEAT_CAPABILITY_KEYBOARD;
	if (has_kb && !app->keyboard) {
		app->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app);
	} else if (!has_kb && app->keyboard) {
		wl_keyboard_release(app->keyboard);
		app->keyboard = NULL;
	}
}
static void seat_handle_name(void *d, struct wl_seat *s, const char *n) {
	(void)d; (void)s; (void)n;
}
static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
                                    const char *interface, uint32_t version) {
	(void)version;
	app_t *app = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		app->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(app->wm_base, &wm_base_listener, app);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
		wl_seat_add_listener(app->seat, &seat_listener, app);
	}
}
static void registry_handle_global_remove(void *d, struct wl_registry *r, uint32_t n) {
	(void)d; (void)r; (void)n;
}
static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

/* ---------------------------------------------------------------------- */

int main(void) {
	app_t app = {0};
	app.width = INIT_WIDTH;
	app.height = INIT_HEIGHT;
	app.selected_section = 0;
	app.font_desc = pango_font_description_from_string(FONT_DESC);
	app.menubar = swl_menubar_new(app.width, MENUS, N_MENUS);

	app.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	app.display = wl_display_connect(NULL);
	if (!app.display) {
		fprintf(stderr, "swlconfig: não consegui conectar ao compositor "
		                 "(WAYLAND_DISPLAY definido?)\n");
		return 1;
	}

	app.registry = wl_display_get_registry(app.display);
	wl_registry_add_listener(app.registry, &registry_listener, &app);
	wl_display_roundtrip(app.display);

	if (!app.compositor || !app.shm || !app.wm_base) {
		fprintf(stderr, "swlconfig: compositor não oferece wl_compositor/"
		                 "wl_shm/xdg_wm_base\n");
		return 1;
	}

	app.surface = wl_compositor_create_surface(app.compositor);
	app.xdg_surface = xdg_wm_base_get_xdg_surface(app.wm_base, app.surface);
	xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);
	app.xdg_toplevel = xdg_surface_get_toplevel(app.xdg_surface);
	xdg_toplevel_add_listener(app.xdg_toplevel, &xdg_toplevel_listener, &app);
	xdg_toplevel_set_title(app.xdg_toplevel, "Configurações");
	xdg_toplevel_set_app_id(app.xdg_toplevel, "swlconfig");
	wl_surface_commit(app.surface);

	while (!app.configured) {
		if (wl_display_dispatch(app.display) == -1) return 1;
	}

	app.running = true;
	while (app.running) {
		if (wl_display_dispatch(app.display) == -1) break;
	}

	for (int i = 0; i < 2; i++) destroy_shm_buffer(&app.bufs[i]);
	if (app.xkb_state) xkb_state_unref(app.xkb_state);
	if (app.xkb_keymap) xkb_keymap_unref(app.xkb_keymap);
	if (app.xkb_context) xkb_context_unref(app.xkb_context);
	if (app.keyboard) wl_keyboard_release(app.keyboard);
	if (app.xdg_toplevel) xdg_toplevel_destroy(app.xdg_toplevel);
	if (app.xdg_surface) xdg_surface_destroy(app.xdg_surface);
	if (app.surface) wl_surface_destroy(app.surface);
	if (app.seat) wl_seat_release(app.seat);
	if (app.wm_base) xdg_wm_base_destroy(app.wm_base);
	if (app.shm) wl_shm_destroy(app.shm);
	if (app.compositor) wl_compositor_destroy(app.compositor);
	if (app.registry) wl_registry_destroy(app.registry);
	if (app.display) wl_display_disconnect(app.display);
	pango_font_description_free(app.font_desc);
	swl_menubar_free(app.menubar);

	return 0;
}

/*
 * TODO (não fingir que está pronto — S1 pede "casca com seções,
 * ganha função por etapa"; isso é a casca):
 *  - Só "Sobre" tem dado real. As outras 11 seções são placeholder
 *    honesto — cada uma vira uma tarefa própria quando alguém pegar.
 *  - Sem mouse (clicar na sidebar) — só Up/Down. Mesma limitação que
 *    o resto dos apps do catálogo hoje.
 *  - Sem persistência de configuração nenhuma (não muda nada de
 *    verdade no sistema ainda — é só interface).
 *  - Layout fixo (não repagina em resize muito pequeno).
 */
