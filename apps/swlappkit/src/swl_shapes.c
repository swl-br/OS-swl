#include <math.h>
#include "swl_shapes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void swl_rrect(cairo_t *cr, double x, double y, double w, double h, double radius) {
    if (radius <= 0.0) { cairo_rectangle(cr, x, y, w, h); return; }
    double r = radius;
    double max_r = (w < h ? w : h) / 2.0;
    if (r > max_r) r = max_r;
    double x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x1 - r, y0 + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x1 - r, y1 - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x0 + r, y1 - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x0 + r, y0 + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

void swl_rrect_bottom(cairo_t *cr, double x, double y, double w, double h, double radius) {
    /* Mesma estrutura de swl_rrect (já testada), só com raio 0 nos dois
     * cantos de cima -- garante geometria idêntica/confiável em vez de
     * reinventar a direção dos arcos do zero. */
    double r = radius;
    double max_r = (w < h ? w : h) / 2.0;
    if (r > max_r) r = max_r;
    double x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x1, y0, 0, -M_PI / 2.0, 0);
    cairo_arc(cr, x1 - r, y1 - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x0 + r, y1 - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x0, y0, 0, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}
