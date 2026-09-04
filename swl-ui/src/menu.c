#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "desktop.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define SWL_MENU_WIDTH      190
#define SWL_MENU_ITEM_H     26
#define SWL_MENU_PAD        6
#define SWL_MENU_MAX_ITEMS  32
#define SWL_MENU_MARGIN     4 /* espaço entre o topo do menu e a taskbar */

struct swl_menu_row {
	int y, h;
	char command[64];
};

struct swl_menu {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer;
	bool open;
	int x, y; /* posição de layout do canto superior-esquerdo do popup */
	int width, height;
	int item_count;
	struct swl_menu_row rows[SWL_MENU_MAX_ITEMS];
};

static void menu_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_menu *menu = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, 0.5, 0.5, width - 1, height - 1);
	cairo_stroke(cr);

	for (int i = 0; i < menu->item_count; i++) {
		struct swl_menu_row *row = &menu->rows[i];
		if (i > 0) {
			SWL_SET(cr, SWL_COL_PANEL_BORDER);
			swl_draw_hline(cr, 4, row->y, width - 8, 1);
		}
		SWL_SET(cr, SWL_COL_TEXT);
		const char *label = swl_desktop_app_label(i);
		swl_draw_text(cr, label ? label : "", SWL_MENU_PAD + 10,
			row->y + row->h / 2.0 - 6, 10, SWL_FONT_MONO, false);
	}
}

struct swl_menu *swl_menu_create(struct wlr_scene_tree *parent) {
	struct swl_menu *menu = calloc(1, sizeof(*menu));

	menu->item_count = swl_desktop_app_count();
	if (menu->item_count > SWL_MENU_MAX_ITEMS) {
		menu->item_count = SWL_MENU_MAX_ITEMS;
	}
	menu->width = SWL_MENU_WIDTH;
	menu->height = SWL_MENU_PAD * 2 + menu->item_count * SWL_MENU_ITEM_H;

	for (int i = 0; i < menu->item_count; i++) {
		menu->rows[i].y = SWL_MENU_PAD + i * SWL_MENU_ITEM_H;
		menu->rows[i].h = SWL_MENU_ITEM_H;
		const char *cmd = swl_desktop_app_command(i);
		snprintf(menu->rows[i].command, sizeof(menu->rows[i].command),
			"%s", cmd ? cmd : "");
	}

	menu->tree = wlr_scene_tree_create(parent);
	menu->buffer = swl_buffer_create(menu->tree, menu->width, menu->height,
		menu_draw, menu);

	/* Começa escondido — só aparece quando swl_menu_open()/toggle() for
	 * chamado a partir do clique no botão MENU da taskbar. */
	menu->open = false;
	wlr_scene_node_set_enabled(&menu->tree->node, false);

	return menu;
}

void swl_menu_resize(struct swl_menu *menu, int screen_height) {
	menu->x = 6;
	menu->y = screen_height - SWL_TASKBAR_HEIGHT - menu->height - SWL_MENU_MARGIN;
	if (menu->y < 0) {
		menu->y = 0;
	}
	wlr_scene_node_set_position(&menu->tree->node, menu->x, menu->y);
}

bool swl_menu_is_open(struct swl_menu *menu) {
	return menu->open;
}

void swl_menu_open(struct swl_menu *menu) {
	if (menu->open) {
		return;
	}
	menu->open = true;
	wlr_scene_node_set_enabled(&menu->tree->node, true);
	wlr_scene_node_raise_to_top(&menu->tree->node);
}

void swl_menu_close(struct swl_menu *menu) {
	if (!menu->open) {
		return;
	}
	menu->open = false;
	wlr_scene_node_set_enabled(&menu->tree->node, false);
}

void swl_menu_toggle(struct swl_menu *menu) {
	if (menu->open) {
		swl_menu_close(menu);
	} else {
		swl_menu_open(menu);
	}
}

int swl_menu_hit_test(struct swl_menu *menu, double x, double y) {
	if (!menu->open) {
		return SWL_MENU_HIT_OUTSIDE;
	}
	if (x < menu->x || x >= menu->x + menu->width ||
			y < menu->y || y >= menu->y + menu->height) {
		return SWL_MENU_HIT_OUTSIDE;
	}
	double ly = y - menu->y;
	for (int i = 0; i < menu->item_count; i++) {
		if (ly >= menu->rows[i].y && ly < menu->rows[i].y + menu->rows[i].h) {
			return i;
		}
	}
	return SWL_MENU_HIT_INSIDE;
}

const char *swl_menu_item_command(struct swl_menu *menu, int index) {
	if (index < 0 || index >= menu->item_count) {
		return NULL;
	}
	return menu->rows[index].command;
}

void swl_menu_destroy(struct swl_menu *menu) {
	if (!menu) {
		return;
	}
	wlr_scene_node_destroy(&menu->tree->node);
	free(menu);
}
