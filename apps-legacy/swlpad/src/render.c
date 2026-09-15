/*
 * render.c — desenho do SWLPad via cairo/pango.
 * Texto monoespaçado em grid, cursor de bloco, barra de status embaixo.
 * Paleta: mesma do theme.h do swl-ui (fundo #0b0e14, texto #d4dee6,
 * ciano #6bd1cc) pra identidade visual consistente.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pango/pangocairo.h>
#include "render.h"

#define SWLPAD_FONT "JetBrains Mono, Fira Code, monospace"
#define SWLPAD_FONT_SIZE 13.0

struct swlpad_render {
    cairo_surface_t *surface;
    int width, height;
    int cw, ch;
    int baseline;
};

/* mede a fonte uma vez */
static void measure_font(swlpad_render *r) {
    cairo_t *cr = cairo_create(r->surface);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, "M", -1);
    char desc[128];
    snprintf(desc, sizeof(desc), "%s %.0f", SWLPAD_FONT, SWLPAD_FONT_SIZE);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    int w, h;
    pango_layout_get_pixel_size(layout, &w, &h);
    r->cw = w > 0 ? w : 9;
    r->ch = h > 0 ? h : 18;
    r->baseline = 2;
    g_object_unref(layout);
    cairo_destroy(cr);
}

swlpad_render *swlpad_render_new(int width, int height) {
    swlpad_render *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    r->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(r->surface) != CAIRO_STATUS_SUCCESS) {
        free(r);
        return NULL;
    }
    r->width = width;
    r->height = height;
    measure_font(r);
    return r;
}

void swlpad_render_free(swlpad_render *r) {
    if (!r) return;
    cairo_surface_destroy(r->surface);
    free(r);
}

cairo_surface_t *swlpad_render_surface(swlpad_render *r) { return r->surface; }
int swlpad_render_cell_w(swlpad_render *r) { return r->cw; }
int swlpad_render_cell_h(swlpad_render *r) { return r->ch; }

void swlpad_render_draw(swlpad_render *r, swlpad_buffer *buf,
        const char *filename, bool cursor_on) {
    cairo_t *cr = cairo_create(r->surface);

    /* fundo geral (#0b0e14) */
    cairo_set_source_rgb(cr, 0.043, 0.055, 0.078);
    cairo_paint(cr);

    /* pega o texto inteiro do buffer */
    size_t len = swlpad_buffer_length(buf);
    char *text = malloc(len + 1);
    swlpad_buffer_get_text(buf, text);

    size_t cursor = swlpad_buffer_cursor(buf);
    int cur_line = swlpad_buffer_cursor_line(buf);
    int cur_col = swlpad_buffer_cursor_col(buf);

    /* desenha linha a linha */
    PangoLayout *layout = pango_cairo_create_layout(cr);
    char desc[128];
    snprintf(desc, sizeof(desc), "%s %.0f", SWLPAD_FONT, SWLPAD_FONT_SIZE);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);

    int max_rows = (r->height - SWLPAD_STATUS_H - 2 * SWLPAD_PAD) / r->ch;
    int row = 0;
    size_t pos = 0;
    /* cursor de bloco: posição dele na tela */
    int cursor_screen_row = -1, cursor_screen_col = -1;

    while (pos <= len && row < max_rows) {
        size_t line_end = pos;
        while (line_end < len && text[line_end] != '\n') line_end++;
        size_t line_len = line_end - pos;

        /* desenha o texto da linha */
        if (line_len > 0) {
            cairo_set_source_rgb(cr, 0.83, 0.87, 0.90);  /* #d4dee6 */
            pango_layout_set_text(layout, text + pos, (int)line_len);
            cairo_move_to(cr, SWLPAD_PAD, SWLPAD_PAD + row * r->ch + r->baseline);
            pango_cairo_show_layout(cr, layout);
        }

        /* o cursor está nesta linha? */
        if (cursor >= pos && cursor <= line_end && cursor_screen_row < 0) {
            cursor_screen_row = row;
            cursor_screen_col = (int)(cursor - pos);
        }

        row++;
        pos = line_end + 1;  /* pula o '\n' */
    }

    /* cursor de bloco */
    if (cursor_on && cursor_screen_row >= 0) {
        cairo_set_source_rgba(cr, 0.42, 0.82, 0.80, 0.85);  /* #6bd1cc */
        cairo_rectangle(cr,
            SWLPAD_PAD + cursor_screen_col * r->cw,
            SWLPAD_PAD + cursor_screen_row * r->ch,
            r->cw, r->ch);
        cairo_fill(cr);
        /* caractere sob o cursor (se houver) na cor de fundo */
        if (cursor < len && text[cursor] != '\n') {
            char chbuf[2] = { text[cursor], 0 };
            cairo_set_source_rgb(cr, 0.043, 0.055, 0.078);
            pango_layout_set_text(layout, chbuf, 1);
            cairo_move_to(cr, SWLPAD_PAD + cursor_screen_col * r->cw,
                          SWLPAD_PAD + cursor_screen_row * r->ch + r->baseline);
            pango_cairo_show_layout(cr, layout);
        }
    }

    g_object_unref(layout);
    free(text);

    /* barra de status */
    int sy = r->height - SWLPAD_STATUS_H;
    cairo_set_source_rgb(cr, 0.055, 0.070, 0.098);  /* #0e1219 */
    cairo_rectangle(cr, 0, sy, r->width, SWLPAD_STATUS_H);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.20, 0.55, 0.58);  /* borda ciano sutil */
    cairo_rectangle(cr, 0, sy, r->width, 1);
    cairo_fill(cr);

    PangoLayout *sl = pango_cairo_create_layout(cr);
    PangoFontDescription *sfd = pango_font_description_from_string(
        SWLPAD_FONT " 10");
    pango_layout_set_font_description(sl, sfd);
    pango_font_description_free(sfd);

    cairo_set_source_rgb(cr, 0.42, 0.82, 0.80);  /* ciano */
    cairo_move_to(cr, 8, sy + 4);
    pango_layout_set_text(sl, filename && filename[0] ? filename : "(novo arquivo)", -1);
    pango_cairo_show_layout(cr, sl);

    char pos_str[64];
    snprintf(pos_str, sizeof(pos_str), "Ln %d, Col %d", cur_line + 1, cur_col + 1);
    cairo_set_source_rgb(cr, 0.45, 0.50, 0.56);  /* dim */
    int pw, ph;
    pango_layout_set_text(sl, pos_str, -1);
    pango_layout_get_pixel_size(sl, &pw, &ph);
    cairo_move_to(cr, r->width - pw - 8, sy + 4);
    pango_cairo_show_layout(cr, sl);

    g_object_unref(sl);
    cairo_destroy(cr);
    cairo_surface_flush(r->surface);
}
