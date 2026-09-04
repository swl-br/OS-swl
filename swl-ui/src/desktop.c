#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include "desktop.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define ICON_GLYPH_SIZE 40
#define ICON_CELL_W     84
#define ICON_CELL_H     92
#define ICON_MARGIN_X   20
#define ICON_MARGIN_Y   16
#define ICON_COLS       2

struct swl_icon_def {
	enum swl_icon_kind kind;
	const char *label;
	const char *command;
	const char *icon_file; /* nome do PNG em assets/icons/, sem caminho (ex.: "tswl.png") */
};

/* Conjunto padrão de ícones, na mesma ordem/agrupamento do mockup. Ajuste
 * livremente — cada linha vira um ícone clicável na área de trabalho. */
static const struct swl_icon_def default_icons[] = {
	{ SWL_ICON_TERMINAL, "TSWL",        "tswl",        "tswl.png" },
	{ SWL_ICON_EDITOR,   "SWLPAD",      "swlpad",      "swlpad.png" },
	{ SWL_ICON_FOLDER,   "ARQUIVOS",    "swlfiles",    "arquivos.png" },
	{ SWL_ICON_BOOK,     "SEBRE",       "sebre",       "sebre.png" },
	{ SWL_ICON_CONFIG,   "CONFIG",      "swlconfig",   "config.png" },
	{ SWL_ICON_CHIP,     "DRIVERS",     "swldrivers",  "drivers.png" },
	{ SWL_ICON_NETWORK,  "REDE",        "swlnet",      "rede.png" },
	{ SWL_ICON_MONITOR,  "SISTEMA",     "swlsysinfo",  "sistema.png" },
	{ SWL_ICON_MEDIA,    "MEDIAPLAYER", "swlmedia",    "mediaplayer.png" },
	{ SWL_ICON_IMAGE,    "IMAGENS",     "swlimages",   "imagens.png" },
	{ SWL_ICON_GAME,     "JOGOS",       "swlgames",    "jogos.png" },
	{ SWL_ICON_ARCHIVE,  "COMPACTAR",   "swlarchive",  "compactar.png" },
	{ SWL_ICON_TRASH,    "LIXEIRA",     "swltrash",    "lixeira.png" },
};
#define N_DEFAULT_ICONS (sizeof(default_icons) / sizeof(default_icons[0]))

struct swl_desktop_icon_node {
	int x, y, w, h;
	char command[64];
};

struct swl_desktop {
	struct wlr_scene_tree *tree;
	struct swl_desktop_icon_node icons[N_DEFAULT_ICONS];
	int count;
};

/* ---------------------------------------------------------------------
 * Glifos vetoriais simples (estilo linha, monocromático + cor de destaque).
 * Cada função desenha dentro de um quadrado lógico de lado `s`, com o
 * cairo_t já transladado para a origem do ícone.
 * --------------------------------------------------------------------- */

static void glyph_terminal(cairo_t *cr, double s) {
	cairo_rectangle(cr, 0, 0, s, s * 0.78);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.12, s * 0.30);
	cairo_line_to(cr, s * 0.30, s * 0.42);
	cairo_line_to(cr, s * 0.12, s * 0.54);
	cairo_move_to(cr, s * 0.36, s * 0.54);
	cairo_line_to(cr, s * 0.62, s * 0.54);
	cairo_stroke(cr);
}

static void glyph_editor(cairo_t *cr, double s) {
	cairo_move_to(cr, s * 0.15, 0);
	cairo_line_to(cr, s * 0.70, 0);
	cairo_line_to(cr, s * 0.85, s * 0.18);
	cairo_line_to(cr, s * 0.85, s * 0.90);
	cairo_line_to(cr, s * 0.15, s * 0.90);
	cairo_close_path(cr);
	cairo_stroke(cr);
	for (int i = 0; i < 3; i++) {
		double ly = s * (0.38 + i * 0.14);
		cairo_move_to(cr, s * 0.28, ly);
		cairo_line_to(cr, s * 0.70, ly);
	}
	cairo_stroke(cr);
}

static void glyph_folder(cairo_t *cr, double s) {
	cairo_move_to(cr, 0, s * 0.22);
	cairo_line_to(cr, s * 0.35, s * 0.22);
	cairo_line_to(cr, s * 0.45, s * 0.34);
	cairo_line_to(cr, s, s * 0.34);
	cairo_line_to(cr, s, s * 0.86);
	cairo_line_to(cr, 0, s * 0.86);
	cairo_close_path(cr);
	cairo_fill_preserve(cr);
	cairo_stroke(cr);
}

static void glyph_book(cairo_t *cr, double s) {
	cairo_rectangle(cr, s * 0.05, s * 0.08, s * 0.42, s * 0.80);
	cairo_fill_preserve(cr);
	cairo_stroke(cr);
	cairo_rectangle(cr, s * 0.50, s * 0.08, s * 0.42, s * 0.80);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.47, s * 0.08);
	cairo_line_to(cr, s * 0.47, s * 0.88);
	cairo_stroke(cr);
}

static void glyph_config(cairo_t *cr, double s) {
	double cx = s / 2, cy = s / 2, r = s * 0.20;
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	cairo_stroke(cr);
	for (int i = 0; i < 8; i++) {
		double a = i * M_PI / 4;
		cairo_move_to(cr, cx + cos(a) * r * 1.3, cy + sin(a) * r * 1.3);
		cairo_line_to(cr, cx + cos(a) * r * 1.7, cy + sin(a) * r * 1.7);
	}
	cairo_stroke(cr);
}

static void glyph_chip(cairo_t *cr, double s) {
	double m = s * 0.22;
	cairo_rectangle(cr, m, m, s - 2 * m, s - 2 * m);
	cairo_stroke(cr);
	for (int i = 0; i < 3; i++) {
		double p = m + (s - 2 * m) * (0.2 + i * 0.3);
		cairo_move_to(cr, 0, p); cairo_line_to(cr, m, p);
		cairo_move_to(cr, s - m, p); cairo_line_to(cr, s, p);
		cairo_move_to(cr, p, 0); cairo_line_to(cr, p, m);
		cairo_move_to(cr, p, s - m); cairo_line_to(cr, p, s);
	}
	cairo_stroke(cr);
}

static void glyph_network(cairo_t *cr, double s) {
	double cx = s / 2, cy = s * 0.85;
	for (int i = 1; i <= 3; i++) {
		cairo_arc(cr, cx, cy, s * 0.22 * i, M_PI * 1.2, M_PI * 1.8);
		cairo_stroke(cr);
	}
	cairo_arc(cr, cx, cy, s * 0.04, 0, 2 * M_PI);
	cairo_fill(cr);
}

static void glyph_monitor(cairo_t *cr, double s) {
	cairo_rectangle(cr, 0, 0, s, s * 0.68);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.35, s * 0.68);
	cairo_line_to(cr, s * 0.30, s * 0.90);
	cairo_line_to(cr, s * 0.70, s * 0.90);
	cairo_line_to(cr, s * 0.65, s * 0.68);
	cairo_stroke(cr);
}

static void glyph_media(cairo_t *cr, double s) {
	cairo_rectangle(cr, 0, s * 0.10, s, s * 0.70);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.40, s * 0.28);
	cairo_line_to(cr, s * 0.68, s * 0.45);
	cairo_line_to(cr, s * 0.40, s * 0.62);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void glyph_image(cairo_t *cr, double s) {
	cairo_rectangle(cr, 0, 0, s, s * 0.80);
	cairo_stroke(cr);
	cairo_arc(cr, s * 0.25, s * 0.25, s * 0.08, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_move_to(cr, s * 0.05, s * 0.75);
	cairo_line_to(cr, s * 0.40, s * 0.40);
	cairo_line_to(cr, s * 0.65, s * 0.60);
	cairo_line_to(cr, s * 0.85, s * 0.40);
	cairo_line_to(cr, s * 0.95, s * 0.75);
	cairo_stroke(cr);
}

static void glyph_game(cairo_t *cr, double s) {
	cairo_move_to(cr, s * 0.15, s * 0.30);
	cairo_line_to(cr, s * 0.85, s * 0.30);
	cairo_line_to(cr, s * 0.95, s * 0.75);
	cairo_line_to(cr, s * 0.70, s * 0.75);
	cairo_line_to(cr, s * 0.60, s * 0.60);
	cairo_line_to(cr, s * 0.40, s * 0.60);
	cairo_line_to(cr, s * 0.30, s * 0.75);
	cairo_line_to(cr, s * 0.05, s * 0.75);
	cairo_close_path(cr);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.27, s * 0.45); cairo_line_to(cr, s * 0.27, s * 0.58);
	cairo_move_to(cr, s * 0.205, s * 0.515); cairo_line_to(cr, s * 0.335, s * 0.515);
	cairo_stroke(cr);
}

static void glyph_archive(cairo_t *cr, double s) {
	cairo_rectangle(cr, s * 0.10, s * 0.05, s * 0.80, s * 0.90);
	cairo_stroke(cr);
	for (int i = 0; i < 4; i++) {
		double y = s * (0.15 + i * 0.15);
		cairo_move_to(cr, s * 0.45, y);
		cairo_line_to(cr, s * 0.55, y);
	}
	cairo_stroke(cr);
}

static void glyph_trash(cairo_t *cr, double s) {
	cairo_move_to(cr, s * 0.20, s * 0.20);
	cairo_line_to(cr, s * 0.28, s * 0.92);
	cairo_line_to(cr, s * 0.72, s * 0.92);
	cairo_line_to(cr, s * 0.80, s * 0.20);
	cairo_close_path(cr);
	cairo_stroke(cr);
	cairo_move_to(cr, s * 0.10, s * 0.20);
	cairo_line_to(cr, s * 0.90, s * 0.20);
	cairo_stroke(cr);
	cairo_rectangle(cr, s * 0.38, s * 0.05, s * 0.24, s * 0.15);
	cairo_stroke(cr);
}

/* Procura o PNG do ícone nos mesmos lugares óbvios que swlwm.c já usa pra
 * achar o wallpaper (pasta assets/ do repo, ou instalado no sistema).
 * Retorna caminho absoluto/relativo utilizável, ou string vazia se não
 * achar nada — nesse caso o chamador cai pro glifo vetorial de sempre. */
static void resolve_icon_path(const char *filename, char *out, size_t out_size) {
	out[0] = '\0';
	if (!filename || !filename[0]) {
		return;
	}
	char candidate[PATH_MAX];
	const char *dirs[] = {
		"assets/icons/",
		"../assets/icons/",
		"/usr/local/share/swl-ui/icons/",
		"/usr/share/swl-ui/icons/",
	};
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		snprintf(candidate, sizeof(candidate), "%s%s", dirs[i], filename);
		if (access(candidate, R_OK) == 0) {
			snprintf(out, out_size, "%s", candidate);
			return;
		}
	}
}

struct swl_icon_draw_ctx {
	enum swl_icon_kind kind;
	char label[32];
	char icon_path[PATH_MAX];
};

static void icon_cell_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_icon_draw_ctx *ctx = data;

	double gs = ICON_GLYPH_SIZE;
	double gx = (width - gs) / 2.0;
	double gy = 4;

	/* Ícone de arquivo (PNG), se existir, no lugar do glifo desenhado à
	 * mão. Mantém compatibilidade total: sem o arquivo, cai no glifo de
	 * sempre — nenhum ícone fica quebrado por falta de asset. */
	if (ctx->icon_path[0]) {
		cairo_surface_t *img = cairo_image_surface_create_from_png(ctx->icon_path);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			int iw = cairo_image_surface_get_width(img);
			int ih = cairo_image_surface_get_height(img);
			if (iw > 0 && ih > 0) {
				cairo_save(cr);
				cairo_translate(cr, gx, gy);
				cairo_scale(cr, gs / iw, gs / ih);
				cairo_set_source_surface(cr, img, 0, 0);
				cairo_paint(cr);
				cairo_restore(cr);
				cairo_surface_destroy(img);
				goto draw_label;
			}
		}
		cairo_surface_destroy(img);
	}

	cairo_save(cr);
	cairo_translate(cr, gx, gy);
	cairo_set_line_width(cr, 1.6);

	switch (ctx->kind) {
	case SWL_ICON_TERMINAL: SWL_SET(cr, SWL_COL_ACCENT_CYAN);   glyph_terminal(cr, gs); break;
	case SWL_ICON_EDITOR:   SWL_SET(cr, SWL_COL_TEXT);          glyph_editor(cr, gs);   break;
	case SWL_ICON_FOLDER:   SWL_SET(cr, SWL_COL_ACCENT_GOLD);   glyph_folder(cr, gs);   break;
	case SWL_ICON_BOOK:     SWL_SET(cr, SWL_COL_ACCENT_PURPLE); glyph_book(cr, gs);     break;
	case SWL_ICON_CONFIG:   SWL_SET(cr, SWL_COL_TEXT_DIM);      glyph_config(cr, gs);   break;
	case SWL_ICON_CHIP:     SWL_SET(cr, SWL_COL_ACCENT_CYAN);   glyph_chip(cr, gs);     break;
	case SWL_ICON_NETWORK:  SWL_SET(cr, SWL_COL_ACCENT_CYAN);   glyph_network(cr, gs);  break;
	case SWL_ICON_MONITOR:  SWL_SET(cr, SWL_COL_TEXT);          glyph_monitor(cr, gs);  break;
	case SWL_ICON_MEDIA:    SWL_SET(cr, SWL_COL_ACCENT_PURPLE); glyph_media(cr, gs);    break;
	case SWL_ICON_IMAGE:    SWL_SET(cr, SWL_COL_TEXT);          glyph_image(cr, gs);    break;
	case SWL_ICON_GAME:     SWL_SET(cr, SWL_COL_ACCENT_GOLD);   glyph_game(cr, gs);     break;
	case SWL_ICON_ARCHIVE:  SWL_SET(cr, SWL_COL_TEXT_DIM);      glyph_archive(cr, gs);  break;
	case SWL_ICON_TRASH:    SWL_SET(cr, SWL_COL_TEXT_DIM);      glyph_trash(cr, gs);    break;
	}
	cairo_restore(cr);

draw_label:
	;
	double lw, lh;
	swl_text_extents(ctx->label, 9, SWL_FONT_MONO, false, &lw, &lh);
	SWL_SET(cr, SWL_COL_TEXT);
	swl_draw_text(cr, ctx->label, (width - lw) / 2.0, gs + 10, 9, SWL_FONT_MONO, false);
}

struct swl_desktop *swl_desktop_create(struct wlr_scene_tree *parent, int top_offset) {
	struct swl_desktop *desktop = calloc(1, sizeof(*desktop));
	desktop->tree = wlr_scene_tree_create(parent);
	desktop->count = (int)N_DEFAULT_ICONS;

	for (size_t i = 0; i < N_DEFAULT_ICONS; i++) {
		int col = (int)(i % ICON_COLS);
		int row = (int)(i / ICON_COLS);
		int x = ICON_MARGIN_X + col * ICON_CELL_W;
		int y = top_offset + ICON_MARGIN_Y + row * ICON_CELL_H;

		struct swl_icon_draw_ctx ctx = {0};
		ctx.kind = default_icons[i].kind;
		snprintf(ctx.label, sizeof(ctx.label), "%s", default_icons[i].label);
		resolve_icon_path(default_icons[i].icon_file, ctx.icon_path, sizeof(ctx.icon_path));

		/* swl_buffer_create desenha de forma síncrona; não precisa manter
		 * `ctx` viva depois desta chamada (ícones fixos não são redesenhados). */
		struct wlr_scene_buffer *buf = swl_buffer_create(desktop->tree,
			ICON_CELL_W, ICON_CELL_H, icon_cell_draw, &ctx);
		wlr_scene_node_set_position(&buf->node, x, y);

		desktop->icons[i].x = x;
		desktop->icons[i].y = y;
		desktop->icons[i].w = ICON_CELL_W;
		desktop->icons[i].h = ICON_CELL_H;
		snprintf(desktop->icons[i].command, sizeof(desktop->icons[i].command),
			"%s", default_icons[i].command);
	}

	return desktop;
}

const char *swl_desktop_hit_test(struct swl_desktop *desktop, double x, double y) {
	for (int i = 0; i < desktop->count; i++) {
		struct swl_desktop_icon_node *icon = &desktop->icons[i];
		if (x >= icon->x && x < icon->x + icon->w &&
				y >= icon->y && y < icon->y + icon->h) {
			return icon->command;
		}
	}
	return NULL;
}

int swl_desktop_app_count(void) {
	return (int)N_DEFAULT_ICONS;
}

const char *swl_desktop_app_label(int index) {
	if (index < 0 || index >= (int)N_DEFAULT_ICONS) {
		return NULL;
	}
	return default_icons[index].label;
}

const char *swl_desktop_app_command(int index) {
	if (index < 0 || index >= (int)N_DEFAULT_ICONS) {
		return NULL;
	}
	return default_icons[index].command;
}

void swl_desktop_destroy(struct swl_desktop *desktop) {
	if (!desktop) {
		return;
	}
	wlr_scene_node_destroy(&desktop->tree->node);
	free(desktop);
}
