#include <stdio.h>
#include <pango/pangocairo.h>
#include "swl_draw_util.h"

static PangoLayout *swl_make_layout(cairo_t *cr, const char *text,
		double size, const char *font, bool bold) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, text, -1);

	char desc_str[160];
	snprintf(desc_str, sizeof(desc_str), "%s %s%.0f",
		font, bold ? "Bold " : "", size);

	PangoFontDescription *desc = pango_font_description_from_string(desc_str);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	return layout;
}

void swl_draw_text(cairo_t *cr, const char *text, double x, double y,
		double size, const char *font, bool bold) {
	PangoLayout *layout = swl_make_layout(cr, text, size, font, bold);
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
}

void swl_text_extents(const char *text, double size, const char *font,
		bool bold, double *out_w, double *out_h) {
	cairo_surface_t *tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(tmp);
	PangoLayout *layout = swl_make_layout(cr, text, size, font, bold);

	int w, h;
	pango_layout_get_pixel_size(layout, &w, &h);
	if (out_w) {
		*out_w = w;
	}
	if (out_h) {
		*out_h = h;
	}

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
}

void swl_draw_hline(cairo_t *cr, double x, double y, double w, double thickness) {
	cairo_rectangle(cr, x, y, w, thickness);
	cairo_fill(cr);
}
