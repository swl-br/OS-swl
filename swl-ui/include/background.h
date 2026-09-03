#ifndef SWL_BACKGROUND_H
#define SWL_BACKGROUND_H

#include <wlr/types/wlr_scene.h>

/* `png_path` pode ser NULL — nesse caso desenha um fundo escuro com uma
 * grade ciano bem sutil (procedural, sem depender de nenhum arquivo). Se
 * apontar para um PNG (ex: a arte do gato), a imagem é carregada via Cairo
 * (sem libs extras) e ajustada por "cover" (preenche a tela, cortando o
 * excedente, sem distorcer). */
struct wlr_scene_buffer *swl_background_create(struct wlr_scene_tree *parent,
	int width, int height, const char *png_path);
void swl_background_resize(struct wlr_scene_buffer *bg, int width, int height,
	const char *png_path);

#endif /* SWL_BACKGROUND_H */
