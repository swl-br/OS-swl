#ifndef SWL_PANEL_H
#define SWL_PANEL_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

/*
 * Barra superior: logo/versão, menu (visual por enquanto), uso de CPU/RAM
 * lido de /proc (sem libs externas) e data/hora.
 */
struct swl_panel {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer;
	struct wl_event_source *timer;
	int width;

	/* estado acumulado para calcular % de CPU por delta entre leituras */
	unsigned long long prev_idle;
	unsigned long long prev_total;
};

struct swl_panel *swl_panel_create(struct wl_event_loop *loop,
	struct wlr_scene_tree *parent, int width);
void swl_panel_resize(struct swl_panel *panel, int width);
void swl_panel_destroy(struct swl_panel *panel);

#endif /* SWL_PANEL_H */
