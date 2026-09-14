#include <math.h>
#include <string.h>
#include <alloca.h>
#include <pango/pangocairo.h>
#include "swl_misc.h"
#include "swl_shapes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void text_wh(const char *text, double size, int *w, int *h) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[96]; snprintf(desc, sizeof(desc), "%s %.0f", SWL_FONT, size);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_pixel_size(layout, w, h);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}
static void dtext(cairo_t *cr, const char *text, double x, double y, double size, swl_color_t color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[96]; snprintf(desc, sizeof(desc), "%s %.0f", SWL_FONT, size);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    SWL_SET(cr, color);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}
static void ctext(cairo_t *cr, const char *text, double cx, double cy, double size, swl_color_t color) {
    int w, h; text_wh(text, size, &w, &h);
    dtext(cr, text, cx - w / 2.0, cy - h / 2.0, size, color);
}

/* ===== Abas ===== */
#define TAB_PAD_X 16
#define TAB_H 30
double swl_tabs_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      const char *const *labels, int n, int selected) {
    double total_w = 0;
    double *w = alloca(sizeof(double) * n);
    for (int i = 0; i < n; i++) {
        int tw, thh; text_wh(labels[i], 12, &tw, &thh);
        w[i] = tw + TAB_PAD_X * 2;
        total_w += w[i];
    }
    swl_rrect(cr, x, y, total_w + 8, TAB_H, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill(cr);
    double cx = x + 4;
    for (int i = 0; i < n; i++) {
        if (i == selected) {
            swl_rrect(cr, cx, y + 4, w[i], TAB_H - 8, SWL_RADIUS_SMALL - 2);
            SWL_SET(cr, th->panel2);
            cairo_fill(cr);
        }
        ctext(cr, labels[i], cx + w[i] / 2.0, y + TAB_H / 2.0, 12, i == selected ? th->text : th->text_dim);
        cx += w[i];
    }
    return total_w + 8;
}
int swl_tabs_hit(double x, double y, double h, const char *const *labels, int n, double px, double py) {
    if (py < y || py >= y + h) return -1;
    double cx = x + 4;
    for (int i = 0; i < n; i++) {
        int tw, thh; text_wh(labels[i], 12, &tw, &thh);
        double w = tw + TAB_PAD_X * 2;
        if (px >= cx && px < cx + w) return i;
        cx += w;
    }
    return -1;
}

/* ===== Tag ===== */
void swl_tag_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                   const char *label, swl_color_t accent) {
    (void)th;
    int tw, thh; text_wh(label, 11, &tw, &thh);
    double w = tw + 20, h = thh + 8;
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_CONTROL);
    cairo_set_source_rgba(cr, accent.r, accent.g, accent.b, 0.14);
    cairo_fill(cr);
    ctext(cr, label, x + w / 2.0, y + h / 2.0, 11, accent);
}

/* ===== Progresso ===== */
void swl_progress_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                        double w, double h, double value01) {
    if (value01 < 0) value01 = 0;
    if (value01 > 1) value01 = 1;
    swl_rrect(cr, x, y, w, h, h / 2.0);
    SWL_SET(cr, th->panel2);
    cairo_fill(cr);
    if (value01 > 0) {
        swl_rrect(cr, x, y, w * value01, h, h / 2.0);
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
    }
}

/* ===== Tooltip ===== */
void swl_tooltip_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, const char *text) {
    int tw, thh; text_wh(text, 11, &tw, &thh);
    double pad_x = 10, pad_y = 6;
    double w = tw + pad_x * 2, h = thh + pad_y * 2;
    swl_rrect(cr, x, y - h - 6, w, h, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel2);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    dtext(cr, text, x + pad_x, y - h - 6 + pad_y, 11, th->text);
}

/* ===== Banner ===== */
void swl_banner_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, double w,
                      const char *text, swl_banner_kind_t kind) {
    swl_color_t accent = kind == SWL_BANNER_ERROR ? SWL_RED : kind == SWL_BANNER_WARN ? SWL_AMBER : SWL_CYAN;
    swl_rrect(cr, x, y, w, SWL_BANNER_H, SWL_RADIUS_SMALL);
    cairo_set_source_rgba(cr, accent.r, accent.g, accent.b, 0.10);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, accent.r, accent.g, accent.b, 0.35);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    (void)th;
    dtext(cr, text, x + 16, y + SWL_BANNER_H / 2.0 - 7, 12, th->text);
}

/* ===== Menu de contexto ===== */
#define CTX_ITEM_H 28
#define CTX_W 190
double swl_context_menu_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                              const swl_ctxitem_t *items, int n, int hover) {
    double h = n * CTX_ITEM_H + 8;
    swl_rrect(cr, x, y, CTX_W, h, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel2);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    for (int i = 0; i < n; i++) {
        double iy = y + 4 + i * CTX_ITEM_H;
        if (i == hover) {
            swl_rrect(cr, x + 4, iy, CTX_W - 8, CTX_ITEM_H, SWL_RADIUS_SMALL - 3);
            cairo_set_source_rgba(cr, SWL_CYAN.r, SWL_CYAN.g, SWL_CYAN.b, 0.12);
            cairo_fill(cr);
        }
        swl_color_t c = items[i].destructive ? SWL_RED : (items[i].enabled ? th->text : th->text_faint);
        dtext(cr, items[i].label, x + 14, iy + CTX_ITEM_H / 2.0 - 7, 12, c);
    }
    return h;
}
int swl_context_menu_hit(double x, double y, int n, double px, double py) {
    double h = n * CTX_ITEM_H + 8;
    if (px < x || px >= x + CTX_W || py < y || py >= y + h) return -1;
    int idx = (int)((py - y - 4) / CTX_ITEM_H);
    return (idx >= 0 && idx < n) ? idx : -1;
}

/* ===== Accordion ===== */
void swl_accordion_header_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                                double w, double h, const char *label, bool open) {
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill(cr);
    dtext(cr, label, x + 14, y + h / 2.0 - 7, 12, th->text);
    double cx = x + w - 20, cy = y + h / 2.0;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    if (open) cairo_rotate(cr, M_PI);
    cairo_move_to(cr, -4, -2);
    cairo_line_to(cr, 0, 2.5);
    cairo_line_to(cr, 4, -2);
    SWL_SET(cr, th->text_dim);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/* ===== Chip de atalho ===== */
double swl_shortcut_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                          const char *const *keys, int n) {
    double cx = x;
    for (int i = 0; i < n; i++) {
        int tw, thh; text_wh(keys[i], 11, &tw, &thh);
        double w = tw + 14, h = thh + 8;
        swl_rrect(cr, cx, y, w, h, 6);
        SWL_SET(cr, th->panel2);
        cairo_fill_preserve(cr);
        SWL_SET(cr, th->border);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        ctext(cr, keys[i], cx + w / 2.0, y + h / 2.0, 11, th->text);
        cx += w + 6;
        if (i < n - 1) {
            dtext(cr, "+", cx, y + h / 2.0 - 7, 11, th->text_faint);
            cx += 12;
        }
    }
    return cx - x;
}

/* ===== Status com ponto ===== */
void swl_status_dot_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                          const char *label, swl_color_t color) {
    cairo_arc(cr, x + 4, y, 4, 0, 2 * M_PI);
    SWL_SET(cr, color);
    cairo_fill(cr);
    dtext(cr, label, x + 14, y - 7, 12, th->text);
}

/* ===== Contador numérico ===== */
void swl_counter_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                       double w, double h, int value) {
    swl_rrect(cr, x, y, w, h, h / 2.0);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    ctext(cr, "-", x + h / 2.0, y + h / 2.0, 13, th->text_dim);
    char buf[16]; snprintf(buf, sizeof(buf), "%d", value);
    ctext(cr, buf, x + w / 2.0, y + h / 2.0, 12, th->text);
    ctext(cr, "+", x + w - h / 2.0, y + h / 2.0, 13, SWL_CYAN);
}

/* ===== Tabela ===== */
void swl_table_header_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                            double w, const char *const *cols, const double *col_w, int n) {
    (void)w;
    double cx = x;
    for (int i = 0; i < n; i++) {
        dtext(cr, cols[i], cx + 14, y + 10, 10, th->text_faint);
        cx += col_w[i];
    }
    SWL_SET(cr, th->border);
    cairo_rectangle(cr, x, y + 28, cx - x, 1);
    cairo_fill(cr);
}
void swl_table_row_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, const char *const *cells, const double *col_w, int n,
                         const swl_color_t *cell_colors, bool zebra) {
    if (zebra) {
        SWL_SET(cr, th->panel);
        cairo_rectangle(cr, x, y, w, SWL_TABLE_ROW_H);
        cairo_fill(cr);
    }
    double cx = x;
    for (int i = 0; i < n; i++) {
        swl_color_t c = cell_colors ? cell_colors[i] : th->text;
        dtext(cr, cells[i], cx + 14, y + SWL_TABLE_ROW_H / 2.0 - 7, 12, c);
        cx += col_w[i];
    }
}
