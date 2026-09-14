#include <math.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "swl_overlays.h"
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

static void draw_text(cairo_t *cr, const char *text, double x, double y,
                       double size, swl_color_t color) {
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

bool swl_rect_hit(double x, double y, double w, double h, double px, double py) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void swl_modal_draw(cairo_t *cr, const swl_theme_t *th, int win_w, int win_h,
                     const char *title, const char *body,
                     const char *ok_label, swl_btn_kind_t ok_kind,
                     const char *cancel_label,
                     double *ok_x, double *ok_y, double *ok_w, double *ok_h,
                     double *cancel_x, double *cancel_y, double *cancel_w, double *cancel_h,
                     double *panel_x, double *panel_y, double *panel_w, double *panel_h) {
    /* véu */
    cairo_rectangle(cr, 0, 0, win_w, win_h);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_fill(cr);

    double panel_w_ = 300;
    int tw, th_px, bw, bh_px;
    text_wh(title, 13, &tw, &th_px);
    text_wh(body, 11.5, &bw, &bh_px);
    double panel_h_ = 24 + th_px + 10 + bh_px + 20 + 34 + 16;
    double px = (win_w - panel_w_) / 2.0;
    double py = (win_h - panel_h_) / 2.0;
    if (panel_x) { *panel_x = px; *panel_y = py; *panel_w = panel_w_; *panel_h = panel_h_; }

    swl_rrect(cr, px, py, panel_w_, panel_h_, SWL_RADIUS_PANEL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    draw_text(cr, title, px + 20, py + 18, 13, th->text);
    draw_text(cr, body, px + 20, py + 18 + th_px + 10, 11.5, th->text_dim);

    double bh = 34;
    double by = py + panel_h_ - bh - 16;
    double bw_ok = 92, bw_cancel = 92;
    double bx_ok = px + panel_w_ - 20 - bw_ok;

    if (cancel_label) {
        double bx_cancel = bx_ok - 10 - bw_cancel;
        swl_button_draw(cr, th, bx_cancel, by, bw_cancel, bh, cancel_label, SWL_BTN_SECONDARY);
        if (cancel_x) { *cancel_x = bx_cancel; *cancel_y = by; *cancel_w = bw_cancel; *cancel_h = bh; }
    } else if (cancel_x) {
        *cancel_x = *cancel_y = *cancel_w = *cancel_h = 0;
    }
    swl_button_draw(cr, th, bx_ok, by, bw_ok, bh, ok_label, ok_kind);
    if (ok_x) { *ok_x = bx_ok; *ok_y = by; *ok_w = bw_ok; *ok_h = bh; }
}

void swl_toast_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, double w,
                     const char *title, const char *sub, swl_toast_kind_t kind) {
    swl_color_t accent = SWL_CYAN;
    if (kind == SWL_TOAST_WARN) accent = SWL_AMBER;
    else if (kind == SWL_TOAST_ERROR) accent = SWL_RED;
    else if (kind == SWL_TOAST_SUCCESS) accent = SWL_CYAN;

    swl_rrect(cr, x, y, w, SWL_TOAST_H, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    /* barrinha de destaque à esquerda */
    swl_rrect(cr, x, y, 3, SWL_TOAST_H, 1.5);
    SWL_SET(cr, accent);
    cairo_fill(cr);

    draw_text(cr, title, x + 16, y + (sub ? 9 : SWL_TOAST_H / 2.0 - 7), 12, th->text);
    if (sub) draw_text(cr, sub, x + 16, y + 25, 10.5, th->text_dim);
}

void swl_dropdown_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                        double w, double h, const char *value, bool open) {
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_CONTROL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, open ? SWL_CYAN : th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    draw_text(cr, value, x + 16, y + h / 2.0 - 7, 12, th->text);

    /* chevron à direita */
    double cx = x + w - 18, cy = y + h / 2.0;
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

double swl_dropdown_list_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                               double w, const char *const *options, int n, int hover) {
    double item_h = 26, pad_v = 6;
    double h = pad_v * 2 + n * item_h;
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    for (int i = 0; i < n; i++) {
        double iy = y + pad_v + i * item_h;
        if (i == hover) {
            SWL_SET(cr, th->panel2);
            cairo_rectangle(cr, x + 2, iy, w - 4, item_h);
            cairo_fill(cr);
        }
        draw_text(cr, options[i], x + 14, iy + item_h / 2.0 - 7, 12,
                  i == hover ? SWL_CYAN : th->text);
    }
    return h;
}

int swl_dropdown_list_hit(double x, double y, double w, int n, double px, double py) {
    double item_h = 26, pad_v = 6;
    double h = pad_v * 2 + n * item_h;
    if (px < x || px >= x + w || py < y || py >= y + h) return -1;
    int idx = (int)((py - y - pad_v) / item_h);
    return (idx >= 0 && idx < n) ? idx : -1;
}
