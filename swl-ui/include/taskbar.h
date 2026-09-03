#ifndef SWL_TASKBAR_H
#define SWL_TASKBAR_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

#define SWL_TASKBAR_MAX_WINDOWS 16

/*
 * Barra inferior: botão MENU, lista de janelas abertas, bandeja de sistema
 * (placeholders por enquanto) e relógio.
 *
 * Este módulo não conhece `tinywl_server` de propósito — o compositor
 * chama swl_taskbar_set_windows() sempre que a lista de janelas muda
 * (mapear/desmapear/focar), passando só os títulos já prontos.
 */
struct swl_taskbar {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer;
	struct wl_event_source *timer;
	int width;
	int y;

	char *titles[SWL_TASKBAR_MAX_WINDOWS];
	int count;
	int focused;

	/* zonas dos botões de janela calculadas no último desenho (hit-test) */
	int win_btn_x[SWL_TASKBAR_MAX_WINDOWS];
	int win_btn_w[SWL_TASKBAR_MAX_WINDOWS];
};

struct swl_taskbar *swl_taskbar_create(struct wl_event_loop *loop,
	struct wlr_scene_tree *parent, int width, int y);
void swl_taskbar_resize(struct swl_taskbar *tb, int width, int y);
void swl_taskbar_destroy(struct swl_taskbar *tb);

void swl_taskbar_set_windows(struct swl_taskbar *tb,
	const char **titles, int count, int focused_index);

/* Retorna: -1 se acertou o botão MENU, >=0 o índice da janela clicada
 * (posição na lista passada em swl_taskbar_set_windows), -2 se não acertou
 * nada relevante (mas o clique ainda está dentro da faixa Y da taskbar). */
int swl_taskbar_hit_test(struct swl_taskbar *tb, double x, double y);

#endif /* SWL_TASKBAR_H */
