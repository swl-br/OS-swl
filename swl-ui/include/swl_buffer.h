#ifndef SWL_BUFFER_H
#define SWL_BUFFER_H

#include <cairo/cairo.h>
#include <wlr/types/wlr_scene.h>

/*
 * swl_buffer: ponte única entre Cairo e a scene graph do wlroots.
 *
 * Isso substitui a duplicação que existia em text_label.c: qualquer widget
 * (relógio, painel, taskbar, decoração de janela, ícone) desenha seu
 * conteúdo numa função de callback recebendo um cairo_t*, e este módulo
 * cuida de toda a parte "chata" (ARGB8888, wlr_buffer_impl, refcounting).
 *
 * Desempenho: swl_buffer_redraw() reaproveita o nó existente na scene graph
 * (via wlr_scene_buffer_set_buffer), então atualizar um relógio a cada
 * segundo não recria/realoca a árvore de cena inteira — só troca o pixel
 * buffer por trás do nó já existente.
 */

typedef void (*swl_draw_fn)(cairo_t *cr, int width, int height, void *data);

/* Cria um novo nó na scene graph, desenhado uma vez com `draw`. */
struct wlr_scene_buffer *swl_buffer_create(struct wlr_scene_tree *parent,
	int width, int height, swl_draw_fn draw, void *data);

/* Redesenha um scene_buffer já existente, sem recriar o nó (mantém posição,
 * z-order e qualquer listener já conectado a ele). */
void swl_buffer_redraw(struct wlr_scene_buffer *scene_buffer,
	int width, int height, swl_draw_fn draw, void *data);

#endif /* SWL_BUFFER_H */
