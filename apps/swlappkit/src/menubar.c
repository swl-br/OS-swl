/*
 * menubar.c — barra de menu com seta retrátil + busca embutida (M2).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pango/pangocairo.h>
#include "menubar.h"
#include "swl_shapes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SWL_MB_FONT_SIZE 11.0
#define SWL_MB_ITEM_H 24
#define SWL_MB_PAD_X 8
#define SWL_MB_DROPDOWN_PAD_V 6
#define SWL_MB_MAX_ITEM_W 220
#define SWL_MB_FONT_DESC SWL_FONT " 11.0"

#define ARROW_W        28
#define ARROW_ANIM_MS  180.0
#define SEARCH_W       150
#define SEARCH_H       20
#define SEARCH_PAD     8

struct swl_menubar {
    int width;
    int bar_h;
    const swl_menu *menus;
    int menu_count;

    bool expanded;   /* alvo: true = aberta, false = só a seta */
    double anim;     /* 0 = colapsada, 1 = totalmente aberta */

    int hover_menu, open_menu, hover_item, key_item;

    int *title_x, *title_w;
    int total_w;

    /* busca (ponteiros do app, não copiados) */
    const char *search_text;
    bool search_focused;
    const swl_search_hit *search_hits;
    int search_hit_count;
    int search_hover;
};

static void free_geoms(struct swl_menubar *mb) {
    free(mb->title_x); free(mb->title_w);
    mb->title_x = NULL; mb->title_w = NULL;
}

static void layout_titles(struct swl_menubar *mb) {
    free_geoms(mb);
    int n = mb->menu_count > 0 ? mb->menu_count : 1;
    mb->title_x = calloc((size_t)n, sizeof(int));
    mb->title_w = calloc((size_t)n, sizeof(int));
    if (!mb->title_x || !mb->title_w) { free_geoms(mb); return; }

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);

    int x = ARROW_W + SWL_MB_PAD_X;
    mb->total_w = 0;
    for (int i = 0; i < mb->menu_count; i++) {
        pango_layout_set_text(layout, mb->menus[i].title, -1);
        int w, h;
        pango_layout_get_pixel_size(layout, &w, &h);
        if (w < 24) w = 24;
        mb->title_x[i] = x;
        mb->title_w[i] = w + SWL_MB_PAD_X * 2;
        x += mb->title_w[i];
        mb->total_w += mb->title_w[i];
    }
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

swl_menubar *swl_menubar_new(int width, const swl_menu *menus, int menu_count) {
    swl_menubar *mb = calloc(1, sizeof(*mb));
    if (!mb) return NULL;
    mb->width = width < 1 ? 1 : width;
    mb->bar_h = SWL_MENUBAR_BAR_H;
    mb->menus = menus;
    mb->menu_count = menu_count > 0 ? menu_count : 0;
    mb->hover_menu = mb->open_menu = mb->hover_item = mb->key_item = -1;
    mb->search_hover = -1;
    mb->expanded = false;
    mb->anim = 0.0;
    if (mb->menu_count > 0) layout_titles(mb);
    return mb;
}

void swl_menubar_free(swl_menubar *mb) {
    if (!mb) return;
    free_geoms(mb);
    free(mb);
}

void swl_menubar_resize(swl_menubar *mb, int width) {
    if (!mb) return;
    mb->width = width < 1 ? 1 : width;
    if (mb->menu_count > 0) layout_titles(mb);
}

void swl_menubar_tick(swl_menubar *mb, int dt_ms) {
    if (!mb) return;
    double target = mb->expanded ? 1.0 : 0.0;
    if (mb->anim == target) return;
    double step = (double)dt_ms / ARROW_ANIM_MS;
    if (mb->anim < target) { mb->anim += step; if (mb->anim > target) mb->anim = target; }
    else { mb->anim -= step; if (mb->anim < target) mb->anim = target; }
}
bool swl_menubar_animating(const swl_menubar *mb) {
    return mb && mb->anim != (mb->expanded ? 1.0 : 0.0);
}
bool swl_menubar_is_expanded(const swl_menubar *mb) { return mb && mb->expanded; }

void swl_menubar_set_search(swl_menubar *mb, const char *text, bool focused,
                             const swl_search_hit *hits, int hit_count) {
    if (!mb) return;
    mb->search_text = text;
    mb->search_focused = focused;
    mb->search_hits = hits;
    mb->search_hit_count = hit_count;
}

void swl_menubar_search_rect(swl_menubar *mb, int *x, int *y, int *w, int *h) {
    if (!mb) return;
    *w = SEARCH_W; *h = SEARCH_H;
    *x = mb->width - SEARCH_W - SEARCH_PAD;
    *y = (mb->bar_h - SEARCH_H) / 2;
}

static void search_dropdown_rect(swl_menubar *mb, int *x, int *y, int *w, int *h) {
    int sx, sy, sw, sh;
    swl_menubar_search_rect(mb, &sx, &sy, &sw, &sh);
    *w = sw + 90;
    *x = sx - (*w - sw);
    if (*x < 0) *x = 0;
    *y = mb->bar_h;
    *h = SWL_MB_DROPDOWN_PAD_V * 2 + mb->search_hit_count * SWL_MB_ITEM_H;
}

int swl_menubar_search_hit_at(swl_menubar *mb, int x, int y) {
    if (!mb || mb->anim < 0.999 || mb->search_hit_count <= 0) return -1;
    int dx, dy, dw, dh;
    search_dropdown_rect(mb, &dx, &dy, &dw, &dh);
    if (x < dx || x >= dx + dw || y < dy || y >= dy + dh) return -1;
    int item = (y - dy - SWL_MB_DROPDOWN_PAD_V) / SWL_MB_ITEM_H;
    return (item >= 0 && item < mb->search_hit_count) ? item : -1;
}

static int hit_title(swl_menubar *mb, int x) {
    if (!mb->title_x || mb->anim < 0.999) return -1;
    for (int i = 0; i < mb->menu_count; i++) {
        if (x >= mb->title_x[i] && x < mb->title_x[i] + mb->title_w[i]) return i;
    }
    return -1;
}

static bool hit_arrow(swl_menubar *mb, int x, int y) {
    return x >= 0 && x < ARROW_W && y >= 0 && y < mb->bar_h;
}

static void dropdown_rect(swl_menubar *mb, int idx, int *x, int *y, int *w, int *h) {
    int item_count = mb->menus[idx].item_count;
    int iw = SWL_MB_MAX_ITEM_W;
    for (int i = 0; i < item_count; i++) {
        if (!mb->menus[idx].items[i].label) continue;
        cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cairo_t *cr = cairo_create(surf);
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, mb->menus[idx].items[i].label, -1);
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        if (tw + SWL_MB_PAD_X * 4 > iw) iw = tw + SWL_MB_PAD_X * 4;
    }
    int dy = mb->bar_h;
    int dx = mb->title_x[idx] < ARROW_W ? ARROW_W : mb->title_x[idx];
    int dh = SWL_MB_DROPDOWN_PAD_V * 2 + item_count * SWL_MB_ITEM_H;
    if (dx + iw > mb->width) dx = mb->width - iw;
    if (dx < 0) dx = 0;
    *x = dx; *y = dy; *w = iw; *h = dh;
}

static int hit_dropdown(swl_menubar *mb, int idx, int x, int y) {
    int dx, dy, dw, dh;
    dropdown_rect(mb, idx, &dx, &dy, &dw, &dh);
    if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
        int item = (y - dy - SWL_MB_DROPDOWN_PAD_V) / SWL_MB_ITEM_H;
        return item < mb->menus[idx].item_count ? item : -1;
    }
    return -1;
}

/* seta: chevron simples que gira 180° entre colapsado (aponta pra
 * baixo, "clique pra abrir") e aberto (aponta pra cima). */
static void draw_arrow(cairo_t *cr, const swl_theme_t *th, int bar_h, double anim) {
    double cx = ARROW_W / 2.0, cy = bar_h / 2.0;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, anim * M_PI);
    double s = 4.5;
    cairo_move_to(cr, -s, -s * 0.4);
    cairo_line_to(cr, 0, s * 0.5);
    cairo_line_to(cr, s, -s * 0.4);
    SWL_SET(cr, anim > 0.5 ? SWL_CYAN : th->text_dim);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_bar(const swl_theme_t *th, cairo_t *cr, swl_menubar *mb, int surface_w) {
    SWL_SET(cr, th->panel);
    cairo_rectangle(cr, 0, 0, surface_w, mb->bar_h);
    cairo_fill(cr);
    SWL_SET(cr, th->border);
    cairo_rectangle(cr, 0, mb->bar_h - 1, surface_w, 1);
    cairo_fill(cr);

    draw_arrow(cr, th, mb->bar_h, mb->anim);

    if (mb->anim <= 0.001) return; /* colapsada: só a seta */

    /* revelação: recorta tudo à direita da seta pela proporção `anim`,
     * dando a sensação de "sair de dentro" dela suavemente. */
    cairo_save(cr);
    double reveal_w = (surface_w - ARROW_W) * mb->anim;
    cairo_rectangle(cr, ARROW_W, 0, reveal_w, mb->bar_h);
    cairo_clip(cr);

    if (mb->menus && mb->menu_count > 0) {
        for (int i = 0; i < mb->menu_count; i++) {
            int x0 = mb->title_x[i], w = mb->title_w[i];
            bool active = (i == mb->open_menu) || (i == mb->hover_menu);
            if (active) {
                SWL_SET(cr, th->panel2);
                cairo_rectangle(cr, x0, 0, w, mb->bar_h - 1);
                cairo_fill(cr);
            }
            SWL_SET(cr, active ? SWL_CYAN : th->text);
            PangoLayout *layout = pango_cairo_create_layout(cr);
            PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
            pango_layout_set_font_description(layout, fd);
            pango_font_description_free(fd);
            pango_layout_set_text(layout, mb->menus[i].title, -1);
            cairo_move_to(cr, x0 + SWL_MB_PAD_X, (mb->bar_h - 10) / 2);
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);
        }
    }

    /* campo de busca, item da própria barra */
    int sx, sy, sw, sh;
    swl_menubar_search_rect(mb, &sx, &sy, &sw, &sh);
    swl_rrect(cr, sx, sy, sw, sh, SWL_RADIUS_CONTROL);
    SWL_SET(cr, th->panel2);
    cairo_fill_preserve(cr);
    SWL_SET(cr, mb->search_focused ? SWL_CYAN : th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    const char *label = (mb->search_text && *mb->search_text) ? mb->search_text : "Buscar";
    SWL_SET(cr, (mb->search_text && *mb->search_text) ? th->text : th->text_faint);
    PangoLayout *slayout = pango_cairo_create_layout(cr);
    PangoFontDescription *sfd = pango_font_description_from_string(SWL_MB_FONT_DESC);
    pango_layout_set_font_description(slayout, sfd);
    pango_font_description_free(sfd);
    pango_layout_set_text(slayout, label, -1);
    cairo_move_to(cr, sx + 10, sy + (sh - 10) / 2.0);
    pango_cairo_show_layout(cr, slayout);
    g_object_unref(slayout);

    cairo_restore(cr);
}

static void draw_menu_dropdown(const swl_theme_t *th, cairo_t *cr, swl_menubar *mb, int idx) {
    int dx, dy, dw, dh;
    dropdown_rect(mb, idx, &dx, &dy, &dw, &dh);
    swl_rrect(cr, dx, dy, dw, dh, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    const swl_menuitem *items = mb->menus[idx].items;
    for (int i = 0; i < mb->menus[idx].item_count; i++) {
        int iy = dy + SWL_MB_DROPDOWN_PAD_V + i * SWL_MB_ITEM_H;
        if (items[i].label == NULL) {
            SWL_SET(cr, th->border);
            cairo_rectangle(cr, dx + 12, iy + SWL_MB_ITEM_H / 2, dw - 24, 1);
            cairo_fill(cr);
            continue;
        }
        bool hovered = (i == mb->hover_item) || (i == mb->key_item);
        if (hovered) {
            SWL_SET(cr, th->panel2);
            cairo_rectangle(cr, dx + 2, iy, dw - 4, SWL_MB_ITEM_H);
            cairo_fill(cr);
        }
        SWL_SET(cr, items[i].enabled ? th->text : th->text_faint);
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, items[i].label, -1);
        cairo_move_to(cr, dx + SWL_MB_PAD_X + 6, iy + (SWL_MB_ITEM_H - 10) / 2);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
}

static void draw_search_dropdown(const swl_theme_t *th, cairo_t *cr, swl_menubar *mb) {
    if (mb->search_hit_count <= 0) return;
    int dx, dy, dw, dh;
    search_dropdown_rect(mb, &dx, &dy, &dw, &dh);
    swl_rrect(cr, dx, dy, dw, dh, SWL_RADIUS_SMALL);
    SWL_SET(cr, th->panel);
    cairo_fill_preserve(cr);
    SWL_SET(cr, th->border);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    for (int i = 0; i < mb->search_hit_count; i++) {
        int iy = dy + SWL_MB_DROPDOWN_PAD_V + i * SWL_MB_ITEM_H;
        if (i == mb->search_hover) {
            SWL_SET(cr, th->panel2);
            cairo_rectangle(cr, dx + 2, iy, dw - 4, SWL_MB_ITEM_H);
            cairo_fill(cr);
        }
        char line[160];
        snprintf(line, sizeof(line), "%s → %s", mb->search_hits[i].label, mb->search_hits[i].category);
        SWL_SET(cr, i == mb->search_hover ? SWL_CYAN : th->text);
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, line, -1);
        cairo_move_to(cr, dx + SWL_MB_PAD_X + 6, iy + (SWL_MB_ITEM_H - 10) / 2);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
}

void swl_menubar_draw(const swl_theme_t *th, swl_menubar *mb, cairo_t *cr, int surface_w) {
    if (!mb) return;
    draw_bar(th, cr, mb, surface_w);
    if (mb->anim < 0.999) return; /* dropdowns só com a barra 100% aberta */
    if (mb->open_menu >= 0) draw_menu_dropdown(th, cr, mb, mb->open_menu);
    draw_search_dropdown(th, cr, mb);
}

void swl_menubar_pointer_motion(swl_menubar *mb, int x, int y) {
    if (!mb) return;
    mb->search_hover = -1;
    if (mb->search_hit_count > 0) {
        int hit = swl_menubar_search_hit_at(mb, x, y);
        if (hit >= 0) { mb->search_hover = hit; return; }
    }
    if (!mb->menus || mb->menu_count < 1) return;
    mb->hover_menu = hit_title(mb, x);
    mb->hover_item = -1;
    if (mb->open_menu >= 0) {
        if (y < mb->bar_h) {
            if (mb->hover_menu >= 0 && mb->hover_menu != mb->open_menu) {
                mb->open_menu = mb->hover_menu;
                mb->key_item = -1;
            }
        } else {
            mb->hover_item = hit_dropdown(mb, mb->open_menu, x, y);
            mb->hover_menu = mb->open_menu;
        }
    }
}

int swl_menubar_pointer_button(swl_menubar *mb, int x, int y, bool pressed) {
    if (!mb) return 0;

    if (pressed && hit_arrow(mb, x, y)) {
        mb->expanded = !mb->expanded;
        if (!mb->expanded) { mb->open_menu = -1; mb->hover_item = -1; mb->key_item = -1; }
        return 0;
    }

    if (!mb->menus || mb->menu_count < 1) return 0;
    int mi = hit_title(mb, x);
    int di = mb->open_menu >= 0 ? hit_dropdown(mb, mb->open_menu, x, y) : -1;

    if (!pressed) {
        if (di >= 0) {
            const swl_menuitem *item = &mb->menus[mb->open_menu].items[di];
            if (item->label && item->enabled) {
                int id = item->id;
                mb->open_menu = -1; mb->hover_menu = mi; mb->hover_item = -1;
                return id;
            }
        }
        return 0;
    }

    if (mb->open_menu >= 0) {
        if (di >= 0) return 0;
        if (mi >= 0) { mb->open_menu = mi; mb->hover_item = -1; mb->key_item = -1; return 0; }
        mb->open_menu = -1; mb->hover_item = -1; mb->key_item = -1;
        return 0;
    }
    if (mi >= 0) { mb->open_menu = mi; mb->hover_item = -1; mb->key_item = -1; return 0; }
    return 0;
}

bool swl_menubar_is_open(swl_menubar *mb) { return mb && mb->open_menu >= 0; }

int swl_menubar_key(swl_menubar *mb, enum swl_menubar_key key) {
    if (!mb || !mb->menus || mb->menu_count < 1 || mb->anim < 0.999) return 0;
    switch (key) {
    case SWL_MENUBAR_KEY_ALT:
        if (mb->open_menu < 0) { mb->open_menu = mb->hover_menu >= 0 ? mb->hover_menu : 0; mb->key_item = -1; }
        else { mb->open_menu = -1; mb->key_item = -1; }
        return 0;
    case SWL_MENUBAR_KEY_ESC:
        if (mb->open_menu >= 0) { mb->open_menu = -1; mb->key_item = -1; }
        return 0;
    case SWL_MENUBAR_KEY_LEFT:
    case SWL_MENUBAR_KEY_RIGHT: {
        if (mb->open_menu < 0) return 0;
        int dir = key == SWL_MENUBAR_KEY_LEFT ? -1 : 1;
        int next = mb->open_menu + dir;
        if (next < 0 || next >= mb->menu_count) next = dir > 0 ? 0 : mb->menu_count - 1;
        mb->open_menu = next; mb->key_item = -1; mb->hover_item = -1;
        return 0;
    }
    case SWL_MENUBAR_KEY_DOWN:
    case SWL_MENUBAR_KEY_UP: {
        if (mb->open_menu < 0) return 0;
        int n = mb->menus[mb->open_menu].item_count;
        if (n < 1) return 0;
        int dir = key == SWL_MENUBAR_KEY_DOWN ? 1 : -1;
        int sel = mb->key_item;
        do { sel = (sel + dir + n) % n; }
        while (mb->menus[mb->open_menu].items[sel].label == NULL && sel != mb->key_item);
        if (mb->menus[mb->open_menu].items[sel].label == NULL) return 0;
        mb->key_item = sel; mb->hover_item = -1;
        return 0;
    }
    case SWL_MENUBAR_KEY_ENTER: {
        if (mb->open_menu < 0) return 0;
        int sel = mb->key_item;
        if (sel < 0) {
            for (int i = 0; i < mb->menus[mb->open_menu].item_count; i++) {
                if (mb->menus[mb->open_menu].items[i].label && mb->menus[mb->open_menu].items[i].enabled) { sel = i; break; }
            }
        }
        if (sel < 0) return 0;
        const swl_menuitem *item = &mb->menus[mb->open_menu].items[sel];
        if (!item->label || !item->enabled) return 0;
        int id = item->id;
        mb->open_menu = -1; mb->key_item = -1;
        return id;
    }
    }
    return 0;
}
