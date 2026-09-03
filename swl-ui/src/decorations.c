#include <stdlib.h>
#include <string.h>
#include "decorations.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define BTN_SIZE   16
#define BTN_GAP    6
#define BTN_MARGIN 6

static void decoration_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_decoration *deco = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	SWL_SET(cr, deco->focused ? SWL_COL_ACCENT_CYAN : SWL_COL_PANEL_BORDER);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, 0.5, 0.5, width - 1, height - 1);
	cairo_stroke(cr);

	SWL_SET(cr, deco->focused ? SWL_COL_TEXT : SWL_COL_TEXT_DIM);
	swl_draw_text(cr, deco->title ? deco->title : "janela",
		8, height / 2.0 - 6, 10, SWL_FONT_MONO, false);

	double bx = width - BTN_MARGIN - BTN_SIZE;
	double by = height / 2.0 - BTN_SIZE / 2.0;

	/* fechar (x) */
	SWL_SET(cr, SWL_COL_DANGER);
	cairo_rectangle(cr, bx, by, BTN_SIZE, BTN_SIZE);
	cairo_stroke(cr);
	cairo_move_to(cr, bx + 4, by + 4);
	cairo_line_to(cr, bx + BTN_SIZE - 4, by + BTN_SIZE - 4);
	cairo_move_to(cr, bx + BTN_SIZE - 4, by + 4);
	cairo_line_to(cr, bx + 4, by + BTN_SIZE - 4);
	cairo_stroke(cr);
	bx -= BTN_SIZE + BTN_GAP;

	/* maximizar (quadrado dentro de quadrado) */
	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	cairo_rectangle(cr, bx, by, BTN_SIZE, BTN_SIZE);
	cairo_stroke(cr);
	cairo_rectangle(cr, bx + 4, by + 4, BTN_SIZE - 8, BTN_SIZE - 8);
	cairo_stroke(cr);
	bx -= BTN_SIZE + BTN_GAP;

	/* minimizar (traço embaixo) */
	SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
	cairo_rectangle(cr, bx, by, BTN_SIZE, BTN_SIZE);
	cairo_stroke(cr);
	cairo_move_to(cr, bx + 4, by + BTN_SIZE - 5);
	cairo_line_to(cr, bx + BTN_SIZE - 4, by + BTN_SIZE - 5);
	cairo_stroke(cr);
}

struct swl_decoration *swl_decoration_create(struct wlr_scene_tree *parent,
		int width, const char *title) {
	struct swl_decoration *deco = calloc(1, sizeof(*deco));
	deco->width = width;
	deco->title = strdup(title ? title : "janela");
	deco->focused = true;
	deco->tree = wlr_scene_tree_create(parent);
	deco->buffer = swl_buffer_create(deco->tree, width, SWL_TITLEBAR_HEIGHT,
		decoration_draw, deco);
	wlr_scene_node_set_position(&deco->tree->node, 0, 0);
	return deco;
}

void swl_decoration_set_title(struct swl_decoration *deco, const char *title) {
	free(deco->title);
	deco->title = strdup(title ? title : "janela");
	swl_buffer_redraw(deco->buffer, deco->width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

void swl_decoration_set_focused(struct swl_decoration *deco, bool focused) {
	if (deco->focused == focused) {
		return;
	}
	deco->focused = focused;
	swl_buffer_redraw(deco->buffer, deco->width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

void swl_decoration_resize(struct swl_decoration *deco, int width) {
	if (width == deco->width || width <= 0) {
		return;
	}
	deco->width = width;
	swl_buffer_redraw(deco->buffer, width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

enum swl_deco_button swl_decoration_hit_test(struct swl_decoration *deco,
		double local_x, double local_y) {
	if (local_y < 0 || local_y >= SWL_TITLEBAR_HEIGHT ||
			local_x < 0 || local_x >= deco->width) {
		return SWL_DECO_NONE;
	}

	double bx = deco->width - BTN_MARGIN - BTN_SIZE;
	double by = SWL_TITLEBAR_HEIGHT / 2.0 - BTN_SIZE / 2.0;

	if (local_x >= bx && local_x < bx + BTN_SIZE &&
			local_y >= by && local_y < by + BTN_SIZE) {
		return SWL_DECO_CLOSE;
	}
	bx -= BTN_SIZE + BTN_GAP;
	if (local_x >= bx && local_x < bx + BTN_SIZE &&
			local_y >= by && local_y < by + BTN_SIZE) {
		return SWL_DECO_MAXIMIZE;
	}
	bx -= BTN_SIZE + BTN_GAP;
	if (local_x >= bx && local_x < bx + BTN_SIZE &&
			local_y >= by && local_y < by + BTN_SIZE) {
		return SWL_DECO_MINIMIZE;
	}
	return SWL_DECO_DRAG;
}

void swl_decoration_destroy(struct swl_decoration *deco) {
	if (!deco) {
		return;
	}
	free(deco->title);
	wlr_scene_node_destroy(&deco->tree->node);
	free(deco);
}
