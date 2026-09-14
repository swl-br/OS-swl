#include <math.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "swl_controls.h"
#include "swl_shapes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- helper interno: texto centralizado com Pango ---------- */
static void centered_text(cairo_t *cr, const char *text, double cx, double cy,
                           double size, swl_color_t color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[96];
    snprintf(desc, sizeof(desc), "%s %.0f", SWL_FONT, size);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);
    SWL_SET(cr, color);
    cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

static void left_text(cairo_t *cr, const char *text, double x, double cy,
                       double size, swl_color_t color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[96];
    snprintf(desc, sizeof(desc), "%s %.0f", SWL_FONT, size);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    int tw, th; (void)tw;
    pango_layout_get_pixel_size(layout, &tw, &th);
    SWL_SET(cr, color);
    cairo_move_to(cr, x, cy - th / 2.0);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

/* ===================== BOTÃO ===================== */
void swl_button_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      double w, double h, const char *label, swl_btn_kind_t kind) {
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_CONTROL);
    swl_color_t text_col;
    if (kind == SWL_BTN_PRIMARY) {
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
        text_col = th->light ? (swl_color_t){0.02,0.13,0.12,1.0} : (swl_color_t){0.016,0.125,0.117,1.0};
    } else if (kind == SWL_BTN_DESTRUCTIVE) {
        cairo_set_source_rgba(cr, SWL_RED.r, SWL_RED.g, SWL_RED.b, 0.45);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        text_col = SWL_RED;
    } else {
        SWL_SET(cr, th->border);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        text_col = th->text;
    }
    centered_text(cr, label, x + w / 2.0, y + h / 2.0, 12, text_col);
}

/* ===================== CAMPO DE TEXTO ===================== */
void swl_textfield_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, double h, const char *text,
                         const char *placeholder, bool focused, bool password) {
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, focused ? SWL_CYAN : th->border);
    cairo_set_line_width(cr, focused ? 1.5 : 1);
    cairo_stroke(cr);

    if (text && *text) {
        char masked[128];
        const char *show = text;
        if (password) {
            size_t n = strlen(text);
            if (n > sizeof(masked) - 1) n = sizeof(masked) - 1;
            memset(masked, 0, sizeof(masked));
            for (size_t i = 0; i < n; i++) masked[i] = '*'; /* UTF-8 bullet fica pra próxima passada */
            show = masked;
        }
        left_text(cr, show, x + 14, y + h / 2.0, 12, th->text);
    } else if (placeholder) {
        left_text(cr, placeholder, x + 14, y + h / 2.0, 12, th->text_faint);
    }
}

/* ===================== CHECKBOX ===================== */
void swl_checkbox_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, bool checked) {
    double s = SWL_CHECKBOX_SIZE;
    swl_rrect(cr, x, y, s, s, 5);
    if (checked) {
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
        swl_color_t mark = th->light ? (swl_color_t){0.02,0.13,0.12,1.0} : (swl_color_t){0.016,0.125,0.117,1.0};
        SWL_SET(cr, mark);
        cairo_set_line_width(cr, 2);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, x + s * 0.24, y + s * 0.52);
        cairo_line_to(cr, x + s * 0.43, y + s * 0.72);
        cairo_line_to(cr, x + s * 0.78, y + s * 0.28);
        cairo_stroke(cr);
    } else {
        SWL_SET(cr, th->border);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);
    }
}

/* ===================== RADIO ===================== */
void swl_radio_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, bool selected) {
    double r = SWL_RADIO_SIZE / 2.0;
    cairo_arc(cr, x + r, y + r, r, 0, 2 * M_PI);
    SWL_SET(cr, selected ? SWL_CYAN : th->border);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    if (selected) {
        cairo_arc(cr, x + r, y + r, r * 0.45, 0, 2 * M_PI);
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
    }
}

/* ===================== TOGGLE ===================== */
#define SWL_TOGGLE_ANIM_MS 140.0

void swl_toggle_init(swl_toggle_t *t, bool initial_on) {
    t->on = initial_on;
    t->anim = initial_on ? 1.0 : 0.0;
}
bool swl_toggle_set(swl_toggle_t *t, bool on) { t->on = on; return t->on; }
void swl_toggle_tick(swl_toggle_t *t, int dt_ms) {
    double target = t->on ? 1.0 : 0.0;
    if (t->anim == target) return;
    double step = (double)dt_ms / SWL_TOGGLE_ANIM_MS;
    if (t->anim < target) { t->anim += step; if (t->anim > target) t->anim = target; }
    else { t->anim -= step; if (t->anim < target) t->anim = target; }
}
bool swl_toggle_animating(const swl_toggle_t *t) { return t->anim != (t->on ? 1.0 : 0.0); }

void swl_toggle_draw(cairo_t *cr, const swl_theme_t *th, const swl_toggle_t *t, double x, double y) {
    double w = SWL_TOGGLE_W, h = SWL_TOGGLE_H, track_r = h / 2.0;
    double a = t->anim;
    swl_color_t off_c = th->panel2, on_c = SWL_CYAN;
    double r = off_c.r + (on_c.r - off_c.r) * a;
    double g = off_c.g + (on_c.g - off_c.g) * a;
    double b = off_c.b + (on_c.b - off_c.b) * a;
    swl_rrect(cr, x, y, w, h, track_r);
    cairo_set_source_rgba(cr, r, g, b, 1.0);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    double knob_r = track_r - 3.0;
    double knob_x = x + track_r + (w - 2.0 * track_r) * a;
    double knob_y = y + h / 2.0;
    cairo_arc(cr, knob_x, knob_y + 1, knob_r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.20);
    cairo_fill(cr);
    cairo_arc(cr, knob_x, knob_y, knob_r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0.98, 0.99, 1.0, 1.0);
    cairo_fill(cr);
}

/* ===================== SEGMENTADO ("balão") ===================== */
void swl_segmented_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, double h, const char *const *options, int n, int selected) {
    swl_rrect(cr, x, y, w, h, SWL_RADIUS_CONTROL);
    SWL_SET(cr, th->panel2);
    cairo_fill(cr);

    double seg_w = w / n;
    if (selected >= 0 && selected < n) {
        double pad = 3;
        swl_rrect(cr, x + selected * seg_w + pad, y + pad, seg_w - 2 * pad, h - 2 * pad, SWL_RADIUS_CONTROL);
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
    }
    swl_color_t sel_text = th->light ? (swl_color_t){0.02,0.13,0.12,1.0} : (swl_color_t){0.016,0.125,0.117,1.0};
    for (int i = 0; i < n; i++) {
        centered_text(cr, options[i], x + i * seg_w + seg_w / 2.0, y + h / 2.0, 11.5,
                      i == selected ? sel_text : th->text_dim);
    }
}

int swl_segmented_hit(double x, double y, double w, double h, int n, double px, double py) {
    if (px < x || px >= x + w || py < y || py >= y + h) return -1;
    double seg_w = w / n;
    int idx = (int)((px - x) / seg_w);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return idx;
}

/* ===================== SLIDER ===================== */
void swl_slider_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      double w, double h, double value01) {
    if (value01 < 0) value01 = 0;
    if (value01 > 1) value01 = 1;
    double track_h = 4;
    double ty = y + h / 2.0 - track_h / 2.0;
    swl_rrect(cr, x, ty, w, track_h, track_h / 2.0);
    SWL_SET(cr, th->panel2);
    cairo_fill(cr);
    if (value01 > 0) {
        swl_rrect(cr, x, ty, w * value01, track_h, track_h / 2.0);
        SWL_SET(cr, SWL_CYAN);
        cairo_fill(cr);
    }
    double knob_r = h / 2.0;
    double kx = x + w * value01;
    cairo_arc(cr, kx, y + h / 2.0, knob_r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.20);
    cairo_fill(cr);
    cairo_arc(cr, kx, y + h / 2.0 - 1, knob_r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0.98, 0.99, 1.0, 1.0);
    cairo_fill(cr);
}
