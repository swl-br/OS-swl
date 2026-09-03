#include <stdlib.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
#include "swl_buffer.h"

struct swl_cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

static void swl_cairo_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct swl_cairo_buffer *buffer = (struct swl_cairo_buffer *)wlr_buffer;
	cairo_surface_destroy(buffer->surface);
	free(buffer);
}

static bool swl_cairo_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct swl_cairo_buffer *buffer = (struct swl_cairo_buffer *)wlr_buffer;
	(void)flags;
	*data = cairo_image_surface_get_data(buffer->surface);
	*stride = cairo_image_surface_get_stride(buffer->surface);
	*format = DRM_FORMAT_ARGB8888;
	return true;
}

static void swl_cairo_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	(void)wlr_buffer;
}

static const struct wlr_buffer_impl swl_cairo_buffer_impl = {
	.destroy = swl_cairo_buffer_destroy,
	.begin_data_ptr_access = swl_cairo_buffer_begin_data_ptr_access,
	.end_data_ptr_access = swl_cairo_buffer_end_data_ptr_access,
};

static struct wlr_buffer *swl_cairo_buffer_render(int width, int height,
		swl_draw_fn draw, void *data) {
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surface);

	/* Começa sempre transparente; quem desenha decide o que pintar por cima. */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	if (draw) {
		draw(cr, width, height, data);
	}

	cairo_destroy(cr);
	cairo_surface_flush(surface);

	struct swl_cairo_buffer *buffer = calloc(1, sizeof(*buffer));
	wlr_buffer_init(&buffer->base, &swl_cairo_buffer_impl, width, height);
	buffer->surface = surface;
	return &buffer->base;
}

struct wlr_scene_buffer *swl_buffer_create(struct wlr_scene_tree *parent,
		int width, int height, swl_draw_fn draw, void *data) {
	struct wlr_buffer *buffer = swl_cairo_buffer_render(width, height, draw, data);
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_create(parent, buffer);
	wlr_buffer_drop(buffer);
	return scene_buffer;
}

void swl_buffer_redraw(struct wlr_scene_buffer *scene_buffer,
		int width, int height, swl_draw_fn draw, void *data) {
	struct wlr_buffer *buffer = swl_cairo_buffer_render(width, height, draw, data);
	wlr_scene_buffer_set_buffer(scene_buffer, buffer);
	wlr_buffer_drop(buffer);
}
