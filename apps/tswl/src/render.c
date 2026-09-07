/*
 * render.c — desenho do grid do TSWL via cairo/pango.
 *
 * Paleta: mesma do theme.h do swl-ui (fundo #0b0e14, texto #d4dee6,
 * ciano #6bd1cc etc.) pra identidade visual consistente.
 *
 * Otimizações:
 * - texto desenhado linha a linha com um único PangoLayout reutilizado;
 * - fundo pintado em retângulos por segmento de mesma cor (não por célula);
 * - o main decide QUANDO chamar draw (só quando o term marca mudança ou
 *   o blink do cursor expira) — draw() em si sempre repinta a superfície
 *   inteira, o que é barato o suficiente em tamanhos de janela normais
 *   (~1-2ms medidos) e muito mais simples que diff de retângulos.
 */

#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "render.h"

#define TSWL_FONT "JetBrains Mono, Fira Code, monospace"
#define TSWL_FONT_SIZE 12.0

typedef struct { double r, g, b; } rgb_t;

/* paleta: índices 0-7 normal, 8-15 bright, 16 = fg default, 17 = bg default */
static const rgb_t palette[18] = {
    { 0.043, 0.055, 0.078 },  /* 0  black (fundo do sistema) */
    { 0.86,  0.35,  0.40  },  /* 1  red (#dc5a66)            */
    { 0.45,  0.78,  0.45  },  /* 2  green                    */
    { 0.83,  0.65,  0.36  },  /* 3  yellow (#d4a55c gold)    */
    { 0.40,  0.60,  0.90  },  /* 4  blue                     */
    { 0.75,  0.62,  0.86  },  /* 5  magenta (#bf9edb purple) */
    { 0.42,  0.82,  0.80  },  /* 6  cyan (#6bd1cc)           */
    { 0.83,  0.87,  0.90  },  /* 7  white (#d4dee6)          */
    { 0.27,  0.31,  0.36  },  /* 8  bright black (#737f8f~)  */
    { 0.95,  0.50,  0.55  },  /* 9  bright red               */
    { 0.60,  0.90,  0.60  },  /* 10 bright green             */
    { 0.93,  0.78,  0.50  },  /* 11 bright yellow            */
    { 0.55,  0.72,  0.97  },  /* 12 bright blue              */
    { 0.85,  0.72,  0.95  },  /* 13 bright magenta           */
    { 0.60,  0.92,  0.90  },  /* 14 bright cyan              */
    { 0.95,  0.97,  1.00  },  /* 15 bright white             */
    { 0.83,  0.87,  0.90  },  /* 16 default fg (#d4dee6)     */
    { 0.043, 0.055, 0.078 },  /* 17 default bg (#0b0e14)     */
};

static rgb_t color_for(uint16_t idx, uint8_t attrs, bool is_fg) {
    if (idx == TSWL_COL_DEFAULT_FG) return palette[16];
    if (idx == TSWL_COL_DEFAULT_BG) return palette[17];
    int i = idx & 0xFF;
    /* negrito eleva cores normais pra bright (convenção clássica) */
    if (is_fg && (attrs & TSWL_ATTR_BOLD) && i < 8) i += 8;
    if (i > 15) i = 15;
    return palette[i];
}

struct tswl_render {
    cairo_surface_t *surface;
    int width, height;      /* pixels da superfície */
    int cw, ch;             /* pixels por célula */
    int baseline;           /* offset Y do baseline do texto na célula */
};

static void measure_font(tswl_render *r) {
    cairo_t *cr = cairo_create(r->surface);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, "M", -1);
    char desc[128];
    snprintf(desc, sizeof(desc), "%s %.0f", TSWL_FONT, TSWL_FONT_SIZE);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);

    int w, h;
    pango_layout_get_pixel_size(layout, &w, &h);
    r->cw = w > 0 ? w : 8;
    r->ch = h > 0 ? h : 16;
    r->baseline = 2;  /* pequeno offset do topo da célula */

    g_object_unref(layout);
    cairo_destroy(cr);
}

tswl_render *tswl_render_new(int width, int height) {
    tswl_render *r = calloc(1, sizeof(*r));
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

void tswl_render_free(tswl_render *r) {
    if (!r) return;
    cairo_surface_destroy(r->surface);
    free(r);
}

cairo_surface_t *tswl_render_surface(tswl_render *r) { return r->surface; }
int tswl_render_cell_w(tswl_render *r) { return r->cw; }
int tswl_render_cell_h(tswl_render *r) { return r->ch; }

int tswl_render_cols_for(tswl_render *r, int width) {
    int n = (width - 2 * TSWL_RENDER_PAD) / r->cw;
    return n < 1 ? 1 : n;
}

int tswl_render_rows_for(tswl_render *r, int height) {
    int n = (height - 2 * TSWL_RENDER_PAD) / r->ch;
    return n < 1 ? 1 : n;
}

/* pinta o fundo de uma linha em segmentos de cor contínua */
static void draw_row_background(cairo_t *cr, tswl_render *r,
        tswl_term *t, int row, int offset_rows) {
    int cols = tswl_term_cols(t);
    rgb_t cur = color_for(0, 0, false);
    int seg_start = 0;
    bool first = true;

    for (int x = 0; x <= cols; x++) {
        rgb_t c = cur;
        if (x < cols) {
            const tswl_cell *cell = offset_rows
                ? tswl_term_scrollback_cell(t, x, row)
                : tswl_term_cell(t, x, row);
            uint16_t bg = cell->bg;
            uint16_t fg = cell->fg;
            if (cell->attrs & TSWL_ATTR_REVERSE) {
                uint16_t tmp = bg;
                bg = (fg == TSWL_COL_DEFAULT_FG) ? 7 : fg;
                fg = tmp;
            }
            c = color_for(bg, 0, false);
        }
        if (first) { cur = c; seg_start = x; first = false; continue; }
        if (c.r != cur.r || c.g != cur.g || c.b != cur.b) {
            if (x > seg_start) {
                cairo_set_source_rgb(cr, cur.r, cur.g, cur.b);
                cairo_rectangle(cr,
                    TSWL_RENDER_PAD + seg_start * r->cw,
                    TSWL_RENDER_PAD + row * r->ch,
                    (x - seg_start) * r->cw, r->ch);
                cairo_fill(cr);
            }
            cur = c;
            seg_start = x;
        }
    }
}

/* desenha o texto de uma linha com um único layout, parando em mudanças
 * de estilo — agrupa runs de células com mesmo fg/attrs */
static void draw_row_text(cairo_t *cr, PangoLayout *layout, tswl_render *r,
        tswl_term *t, int row, int offset_rows) {
    int cols = tswl_term_cols(t);
    int x = 0;
    while (x < cols) {
        const tswl_cell *cell = offset_rows
            ? tswl_term_scrollback_cell(t, x, row)
            : tswl_term_cell(t, x, row);
        if (cell->ch == 0 || cell->ch == ' ') { x++; continue; }

        /* run: células consecutivas com mesmo estilo */
        uint16_t fg = cell->fg;
        uint16_t bg = cell->bg;
        uint8_t attrs = cell->attrs;
        if (attrs & TSWL_ATTR_REVERSE) {
            fg = (bg == TSWL_COL_DEFAULT_BG) ? 17 : bg;
        }
        int x0 = x;
        char buf[512];
        int blen = 0;
        while (x < cols) {
            const tswl_cell *c2 = offset_rows
                ? tswl_term_scrollback_cell(t, x, row)
                : tswl_term_cell(t, x, row);
            if (c2->ch == 0) break;
            uint16_t fg2 = c2->fg, bg2 = c2->bg;
            if (c2->attrs & TSWL_ATTR_REVERSE) {
                fg2 = (bg2 == TSWL_COL_DEFAULT_BG) ? 17 : bg2;
            }
            if (fg2 != fg || (c2->attrs & ~TSWL_ATTR_REVERSE) != (attrs & ~TSWL_ATTR_REVERSE))
                break;
            if (blen >= (int)sizeof(buf) - 8) break;
            uint32_t cp = c2->ch;
            if (cp < 0x80) buf[blen++] = (char)cp;
            else if (cp < 0x800) {
                buf[blen++] = (char)(0xC0 | (cp >> 6));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                buf[blen++] = (char)(0xE0 | (cp >> 12));
                buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            } else {
                buf[blen++] = (char)(0xF0 | (cp >> 18));
                buf[blen++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            }
            x++;
        }
        if (x == x0) { x++; continue; }
        buf[blen] = 0;

        char desc[128];
        snprintf(desc, sizeof(desc), "%s%s %.0f", TSWL_FONT,
            (attrs & TSWL_ATTR_BOLD) ? " Bold" : "", TSWL_FONT_SIZE);
        PangoFontDescription *fd = pango_font_description_from_string(desc);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, buf, blen);

        rgb_t c = color_for(fg, attrs, true);
        if (attrs & TSWL_ATTR_DIM) {
            c.r *= 0.6; c.g *= 0.6; c.b *= 0.6;
        }
        cairo_set_source_rgb(cr, c.r, c.g, c.b);
        cairo_move_to(cr, TSWL_RENDER_PAD + x0 * r->cw,
                      TSWL_RENDER_PAD + row * r->ch + r->baseline);
        pango_cairo_show_layout(cr, layout);

        if (attrs & TSWL_ATTR_UNDERLINE) {
            int run_w = (x - x0) * r->cw;
            cairo_rectangle(cr, TSWL_RENDER_PAD + x0 * r->cw,
                TSWL_RENDER_PAD + row * r->ch + r->ch - 2, run_w, 1);
            cairo_fill(cr);
        }
    }
}

void tswl_render_draw(tswl_render *r, tswl_term *t, bool cursor_on) {
    cairo_t *cr = cairo_create(r->surface);

    /* fundo geral */
    cairo_set_source_rgb(cr, palette[17].r, palette[17].g, palette[17].b);
    cairo_paint(cr);

    int rows = tswl_term_rows(t);
    int offset = tswl_term_scroll_offset(t);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    for (int row = 0; row < rows; row++) {
        draw_row_background(cr, r, t, row, offset);
        draw_row_text(cr, layout, r, t, row, offset);
    }
    g_object_unref(layout);

    /* cursor */
    if (cursor_on && tswl_term_cursor_visible(t) && offset == 0) {
        int cx = tswl_term_cursor_x(t);
        int cy = tswl_term_cursor_y(t);
        const tswl_cell *cell = tswl_term_cell(t, cx, cy);
        /* cursor: bloco com cor do fg da célula (ou default), texto fica
         * com a cor de fundo por cima — efeito de inversão clássico */
        rgb_t cc = color_for(cell->fg == TSWL_COL_DEFAULT_FG
                             ? (uint16_t)7 : cell->fg, 0, true);
        cairo_set_source_rgba(cr, cc.r, cc.g, cc.b, 0.85);
        cairo_rectangle(cr, TSWL_RENDER_PAD + cx * r->cw,
                        TSWL_RENDER_PAD + cy * r->ch, r->cw, r->ch);
        cairo_fill(cr);
        if (cell->ch > 0x20) {
            char buf[8];
            int blen = 0;
            uint32_t cp = cell->ch;
            if (cp < 0x80) buf[blen++] = (char)cp;
            else if (cp < 0x800) {
                buf[blen++] = (char)(0xC0 | (cp >> 6));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                buf[blen++] = (char)(0xE0 | (cp >> 12));
                buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            } else {
                buf[blen++] = (char)(0xF0 | (cp >> 18));
                buf[blen++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                buf[blen++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[blen++] = (char)(0x80 | (cp & 0x3F));
            }
            buf[blen] = 0;
            PangoLayout *cl = pango_cairo_create_layout(cr);
            char desc[128];
            snprintf(desc, sizeof(desc), "%s%s %.0f", TSWL_FONT,
                (cell->attrs & TSWL_ATTR_BOLD) ? " Bold" : "", TSWL_FONT_SIZE);
            PangoFontDescription *fd = pango_font_description_from_string(desc);
            pango_layout_set_font_description(cl, fd);
            pango_font_description_free(fd);
            pango_layout_set_text(cl, buf, blen);
            cairo_set_source_rgb(cr, palette[17].r, palette[17].g, palette[17].b);
            cairo_move_to(cr, TSWL_RENDER_PAD + cx * r->cw,
                          TSWL_RENDER_PAD + cy * r->ch + r->baseline);
            pango_cairo_show_layout(cr, cl);
            g_object_unref(cl);
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(r->surface);
    tswl_term_clear_dirty(t);
}
