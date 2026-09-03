#ifndef SWL_DRAW_UTIL_H
#define SWL_DRAW_UTIL_H

#include <cairo/cairo.h>
#include <stdbool.h>

/* Desenha texto usando Pango (bom fallback de fontes, sem depender de shaping
 * manual). `size` é o tamanho da fonte em pontos, `font` a família desejada. */
void swl_draw_text(cairo_t *cr, const char *text, double x, double y,
	double size, const char *font, bool bold);

/* Mede o texto sem desenhar (usado pra alinhar/centralizar labels). */
void swl_text_extents(const char *text, double size, const char *font,
	bool bold, double *out_w, double *out_h);

void swl_draw_hline(cairo_t *cr, double x, double y, double w, double thickness);

#endif /* SWL_DRAW_UTIL_H */
