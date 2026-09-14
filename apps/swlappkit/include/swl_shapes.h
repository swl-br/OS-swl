#ifndef SWL_APPKIT_SHAPES_H
#define SWL_APPKIT_SHAPES_H
#include <cairo/cairo.h>

/* Path de retângulo arredondado (todos os 4 cantos). radius <= 0 vira
 * canto reto. Clampa o raio pra nunca passar da metade do menor lado. */
void swl_rrect(cairo_t *cr, double x, double y, double w, double h, double radius);

/* Só os cantos de BAIXO arredondados (uso: rodapé do conteúdo do app,
 * pra combinar com o topo que o compositor arredonda -- ver
 * SWL_RADIUS_WINDOW em swl_theme.h). Topo fica reto. */
void swl_rrect_bottom(cairo_t *cr, double x, double y, double w, double h, double radius);

#endif
