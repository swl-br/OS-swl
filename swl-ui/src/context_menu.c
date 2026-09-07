#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context_menu.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define SWL_CTXMENU_WIDTH  180
#define SWL_CTXMENU_ITEM_H 24
#define SWL_CTXMENU_PAD    4

struct swl_context_menu {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer; /* NULL quando fechado (nada desenhado ainda) */
	bool open;
	int x, y, width, height;
	int count;
	char labels[SWL_CTXMENU_MAX_ITEMS][40];
};

static void ctxmenu_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_context_menu *menu = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, 0.5, 0.5, width - 1, height - 1);
	cairo_stroke(cr);

	for (int i = 0; i < menu->count; i++) {
		double ry = SWL_CTXMENU_PAD + i * SWL_CTXMENU_ITEM_H;
		if (i > 0) {
			SWL_SET(cr, SWL_COL_PANEL_BORDER);
			swl_draw_hline(cr, 4, ry, width - 8, 1);
		}
		SWL_SET(cr, SWL_COL_TEXT);
		swl_draw_text(cr, menu->labels[i], SWL_CTXMENU_PAD + 8,
			ry + SWL_CTXMENU_ITEM_H / 2.0 - 6, 10, SWL_FONT_MONO, false);
	}
}

struct swl_context_menu *swl_context_menu_create(struct wlr_scene_tree *parent) {
	struct swl_context_menu *menu = calloc(1, sizeof(*menu));
	menu->tree = wlr_scene_tree_create(parent);
	menu->open = false;
	wlr_scene_node_set_enabled(&menu->tree->node, false);
	return menu;
}

void swl_context_menu_open(struct swl_context_menu *menu, int x, int y,
		int screen_width, int screen_height,
		const char *const *labels, int count) {
	if (count > SWL_CTXMENU_MAX_ITEMS) {
		count = SWL_CTXMENU_MAX_ITEMS;
	}
	menu->count = count;
	for (int i = 0; i < count; i++) {
		snprintf(menu->labels[i], sizeof(menu->labels[i]), "%s", labels[i]);
	}
	menu->width = SWL_CTXMENU_WIDTH;
	menu->height = SWL_CTXMENU_PAD * 2 + count * SWL_CTXMENU_ITEM_H;

	/* Não deixa o popup nascer fora da tela — se não couber pra baixo/
	 * direita a partir do ponto de clique, abre pra cima/esquerda dele. */
	if (x + menu->width > screen_width) {
		x = screen_width - menu->width;
	}
	if (y + menu->height > screen_height) {
		y = y - menu->height;
	}
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	menu->x = x;
	menu->y = y;

	/* Conteúdo dinâmico (varia por chamada) — diferente do menu iniciar,
	 * redesenha o buffer a cada abertura em vez de desenhar uma vez só.
	 * `menu` (não uma cópia local) é o dado passado pro draw callback,
	 * então tem que continuar vivo até a próxima abertura/destroy — é o
	 * caso, já que é um campo do próprio widget. */
	if (menu->buffer) {
		wlr_scene_node_destroy(&menu->buffer->node);
	}
	menu->buffer = swl_buffer_create(menu->tree, menu->width, menu->height,
		ctxmenu_draw, menu);
	wlr_scene_node_set_position(&menu->tree->node, menu->x, menu->y);

	menu->open = true;
	wlr_scene_node_set_enabled(&menu->tree->node, true);
	wlr_scene_node_raise_to_top(&menu->tree->node);
}

void swl_context_menu_close(struct swl_context_menu *menu) {
	if (!menu->open) {
		return;
	}
	menu->open = false;
	wlr_scene_node_set_enabled(&menu->tree->node, false);
}

bool swl_context_menu_is_open(struct swl_context_menu *menu) {
	return menu->open;
}

int swl_context_menu_hit_test(struct swl_context_menu *menu, double x, double y) {
	if (!menu->open) {
		return SWL_CTXMENU_HIT_OUTSIDE;
	}
	if (x < menu->x || x >= menu->x + menu->width ||
			y < menu->y || y >= menu->y + menu->height) {
		return SWL_CTXMENU_HIT_OUTSIDE;
	}
	double ly = y - menu->y;
	int idx = (int)((ly - SWL_CTXMENU_PAD) / SWL_CTXMENU_ITEM_H);
	if (idx >= 0 && idx < menu->count) {
		return idx;
	}
	return SWL_CTXMENU_HIT_INSIDE;
}

void swl_context_menu_destroy(struct swl_context_menu *menu) {
	if (!menu) {
		return;
	}
	wlr_scene_node_destroy(&menu->tree->node);
	free(menu);
}
