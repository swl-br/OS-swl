#include <string.h>
#include <stdio.h>
#include <pango/pangocairo.h>
#include "central.h"

extern void cc_page_draw(cc_app_t *a, cairo_t *cr, int rx, int ry, int rw, int *out_content_h);

void cc_contentgeom(cc_app_t *a, int *sx, int *sy, int *sw, int *rx, int *ry, int *rw, int *bot) {
    int top = SWL_MENUBAR_BAR_H + 12;
    *sx = 0; *sy = SWL_MENUBAR_BAR_H;
    *sw = CC_SIDEBAR_W;
    *rx = CC_SIDEBAR_W + 20;
    *ry = top;
    *rw = a->width - *rx - 20;
    *bot = a->height - (int)SWL_RADIUS_WINDOW;
}

void cc_text_wh(const char *text, double size, int *w, int *h) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[64]; snprintf(desc, sizeof(desc), "%s %.0f", SWL_FONT, size);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_pixel_size(layout, w, h);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void cc_draw_small_caption(cairo_t *cr, const char *text, double x, double y, swl_color_t color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[64]; snprintf(desc, sizeof(desc), "%s 12", SWL_FONT);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    SWL_SET(cr, color);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

bool cc_field_key(cc_field_t *f, struct xkb_state *xkb, uint32_t keycode, xkb_keysym_t sym) {
    if (sym == XKB_KEY_BackSpace) {
        size_t n = strlen(f->buf);
        while (n > 0 && (f->buf[n - 1] & 0xC0) == 0x80) n--;
        if (n > 0) n--;
        f->buf[n] = 0;
        return true;
    }
    char utf8[16];
    int n = xkb_state_key_get_utf8(xkb, keycode, utf8, sizeof(utf8) - 1);
    if (n > 0) {
        utf8[n] = 0;
        size_t cur = strlen(f->buf);
        if (cur + (size_t)n < sizeof(f->buf) - 1 && utf8[0] >= 32) {
            memcpy(f->buf + cur, utf8, (size_t)n + 1);
            return true;
        }
    }
    return false;
}

static void draw_sidebar(cc_app_t *a, cairo_t *cr, int sy, int sw) {
    swl_color_t sep = { a->theme.text_faint.r, a->theme.text_faint.g, a->theme.text_faint.b, 0.5 };
    int row_h = 34, y = sy + 8;
    for (int i = 0; i < CC_CAT_COUNT; i++) {
        bool sel = (i == (int)a->sel);
        if (sel) {
            swl_rrect(cr, 8, y, sw - 16, row_h - 4, SWL_RADIUS_SMALL);
            SWL_SET(cr, a->theme.panel2);
            cairo_fill(cr);
        }
        cairo_move_to(cr, 20, y + row_h / 2.0 - 2);
        SWL_SET(cr, sel ? SWL_CYAN : a->theme.text_dim);
        cairo_arc(cr, 20, y + row_h / 2.0 - 6, 3, 0, 6.283185);
        cairo_fill(cr);
        cairo_move_to(cr, 32, y + row_h / 2.0 - 7);
        {
            cairo_save(cr);
            PangoLayout *l = pango_cairo_create_layout(cr);
            char desc[64]; snprintf(desc, sizeof(desc), "%s 12", SWL_FONT);
            PangoFontDescription *fd = pango_font_description_from_string(desc);
            pango_layout_set_font_description(l, fd);
            pango_font_description_free(fd);
            pango_layout_set_text(l, cc_cat_names[i], -1);
            SWL_SET(cr, sel ? a->theme.text : a->theme.text_dim);
            pango_cairo_show_layout(cr, l);
            g_object_unref(l);
            cairo_restore(cr);
        }
        y += row_h;
    }
    (void)sep;
}

void cc_draw_all(cc_app_t *a) {
    cairo_t *cr = a->cr;
    SWL_SET(cr, a->theme.bg);
    cairo_paint(cr);

    /* quinas de baixo arredondadas, combinando com a decoração do
     * compositor (SWL_RADIUS_WINDOW é a mesma constante dos dois lados) */
    swl_rrect_bottom(cr, 0, 0, a->width, a->height, SWL_RADIUS_WINDOW);
    cairo_clip(cr);
    SWL_SET(cr, a->theme.bg);
    cairo_paint(cr);

    int sx, sy, sw, rx, ry, rw, bot;
    cc_contentgeom(a, &sx, &sy, &sw, &rx, &ry, &rw, &bot);
    (void)sx;

    draw_sidebar(a, cr, sy, sw);

    /* separador vertical sidebar/conteúdo */
    SWL_SET(cr, a->theme.border);
    cairo_rectangle(cr, sw, sy, 1, a->height - sy);
    cairo_fill(cr);

    /* título da categoria + conteúdo, recortado na área de visualização
     * (pra permitir "scroll" sem vazar por cima da sidebar/menubar) */
    cairo_save(cr);
    cairo_rectangle(cr, rx, ry, rw, bot - ry);
    cairo_clip(cr);
    cairo_translate(cr, 0, -a->scroll);

    PangoLayout *tl = pango_cairo_create_layout(cr);
    char tdesc[64]; snprintf(tdesc, sizeof(tdesc), "%s 17", SWL_FONT);
    PangoFontDescription *tfd = pango_font_description_from_string(tdesc);
    pango_layout_set_font_description(tl, tfd);
    pango_font_description_free(tfd);
    pango_layout_set_text(tl, cc_cat_names[a->sel], -1);
    SWL_SET(cr, a->theme.text);
    cairo_move_to(cr, rx, ry);
    pango_cairo_show_layout(cr, tl);
    g_object_unref(tl);

    int content_h = 0;
    cc_page_draw(a, cr, rx, ry + 34, rw, &content_h);
    a->content_h = content_h + 34;
    a->view_h = bot - ry;
    cairo_restore(cr);

    /* scrollbar fina (sempre visível quando há conteúdo maior que a
     * área — versão simplificada; some-ao-parar-de-rolar fica pra uma
     * próxima passada de polimento) */
    if (a->content_h > a->view_h) {
        double h = (bot - ry) * a->view_h / a->content_h;
        if (h < 30) h = 30;
        double frac = a->scroll / (a->content_h - a->view_h);
        double y0 = ry + frac * ((bot - ry) - h);
        swl_rrect(cr, a->width - 8, y0, 4, h, 2);
        cairo_set_source_rgba(cr, a->theme.text_dim.r, a->theme.text_dim.g, a->theme.text_dim.b, 0.4);
        cairo_fill(cr);
    }

    cairo_restore(cr); /* fim do clip de cantos de baixo */

    /* busca: hits + resultados vêm do app, o menubar só desenha/testa */
    swl_menubar_set_search(a->menubar, a->search, a->search_focused, a->search_hits, a->search_nhits);
    swl_menubar_draw(&a->theme, a->menubar, cr, a->width);

    if (a->drop.open) {
        swl_dropdown_list_draw(cr, &a->theme, a->drop.x, a->drop.y + a->drop.h, a->drop.w,
                                a->drop.options, a->drop.nopts, a->drop.hover);
    }

    if (a->dialog.open) {
        double ox, oy, ow, oh, cx, cy, cw, ch, px2, py2, pw2, ph2;
        swl_modal_draw(cr, &a->theme, a->width, a->height, a->dialog.title, a->dialog.body,
                       a->dialog.ok_label, SWL_BTN_PRIMARY, "Cancelar",
                       &ox, &oy, &ow, &oh, &cx, &cy, &cw, &ch, &px2, &py2, &pw2, &ph2);
        if (a->dialog.has_field) {
            swl_textfield_draw(cr, &a->theme, px2 + 20, oy - 44, pw2 - 40, 32,
                                a->dialog.field.buf, a->dialog.field.placeholder,
                                a->dialog.field.focused, a->dialog.field.password);
        }
    }
}
