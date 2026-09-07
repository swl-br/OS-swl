#include <stdio.h>
#include <string.h>
#include <math.h>
#include "background.h"
#include "swl_buffer.h"
#include "theme.h"

struct bg_ctx {
	char png_path[256];
};

static void background_draw(cairo_t *cr, int width, int height, void *data) {
	struct bg_ctx *ctx = data;

	SWL_SET(cr, SWL_COL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	if (ctx && ctx->png_path[0]) {
		cairo_surface_t *img = cairo_image_surface_create_from_png(ctx->png_path);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			int iw = cairo_image_surface_get_width(img);
			int ih = cairo_image_surface_get_height(img);
			if (iw > 0 && ih > 0) {
				double scale = fmax((double)width / iw, (double)height / ih);
				cairo_save(cr);
				cairo_translate(cr,
					(width - iw * scale) / 2.0, (height - ih * scale) / 2.0);
				cairo_scale(cr, scale, scale);
				cairo_set_source_surface(cr, img, 0, 0);
				cairo_paint(cr);
				cairo_restore(cr);
			}
		}
		cairo_surface_destroy(img);
		return;
	}

	/* Fallback procedural: grade sutil ciano sobre fundo escuro. */
	SWL_SET(cr, SWL_COLOR(0.15, 0.35, 0.38, 0.12));
	cairo_set_line_width(cr, 1);
	for (int x = 0; x < width; x += 32) {
		cairo_move_to(cr, x, 0);
		cairo_line_to(cr, x, height);
	}
	for (int y = 0; y < height; y += 32) {
		cairo_move_to(cr, 0, y);
		cairo_line_to(cr, width, y);
	}
	cairo_stroke(cr);
}

struct wlr_scene_buffer *swl_background_create(struct wlr_scene_tree *parent,
		int width, int height, const char *png_path) {
	struct bg_ctx ctx = {0};
	if (png_path) {
		snprintf(ctx.png_path, sizeof(ctx.png_path), "%s", png_path);
	}
	struct wlr_scene_buffer *buf =
		swl_buffer_create(parent, width, height, background_draw, &ctx);
	wlr_scene_node_set_position(&buf->node, 0, 0);
	return buf;
}

void swl_background_resize(struct wlr_scene_buffer *bg, int width, int height,
		const char *png_path) {
	struct bg_ctx ctx = {0};
	if (png_path) {
		snprintf(ctx.png_path, sizeof(ctx.png_path), "%s", png_path);
	}
	swl_buffer_redraw(bg, width, height, background_draw, &ctx);
}
