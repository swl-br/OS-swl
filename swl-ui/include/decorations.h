#ifndef SWL_DECORATIONS_H
#define SWL_DECORATIONS_H

#include <stdbool.h>
#include <wlr/types/wlr_scene.h>

enum swl_deco_button {
	SWL_DECO_NONE = 0,
	SWL_DECO_CLOSE,
	SWL_DECO_MAXIMIZE,
	SWL_DECO_MINIMIZE,
	SWL_DECO_DRAG, /* clicou na barra, fora dos botões: arrastar/mover */
};

struct swl_decoration {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffer;
	int width;
	char *title;
	bool focused;
};

/* `parent` deve ser a scene_tree "wrapper" do toplevel (não a subárvore da
 * superfície xdg em si) — veja o patch de integração no swlwm.c. A decoração
 * é criada na posição local (0,0); o conteúdo da janela fica deslocado
 * SWL_TITLEBAR_HEIGHT pixels abaixo, dentro do mesmo wrapper. */
struct swl_decoration *swl_decoration_create(struct wlr_scene_tree *parent,
	int width, const char *title);
void swl_decoration_set_title(struct swl_decoration *deco, const char *title);
void swl_decoration_set_focused(struct swl_decoration *deco, bool focused);
void swl_decoration_resize(struct swl_decoration *deco, int width);
void swl_decoration_destroy(struct swl_decoration *deco);

/* (local_x, local_y) relativos ao topo-esquerda do wrapper do toplevel. */
enum swl_deco_button swl_decoration_hit_test(struct swl_decoration *deco,
	double local_x, double local_y);

#endif /* SWL_DECORATIONS_H */
