/*
 * render.c - painel do swlsysinfo via cairo/pango.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "render.h"
#include "monitor.h"

#define FONT "JetBrains Mono, Fira Code, monospace"
#define FSZ 12.0
#define PAD 12
#define LINE_H 20
#define HEADER_H 36

struct swlsysinfo_render
{
    cairo_surface_t *surface;
    int width, height;
};

typedef struct
{
    double r, g, b;
} rgb_t;

static const rgb_t BG    = { 0.043, 0.055, 0.078 };
static const rgb_t PANEL = { 0.055, 0.070,  0.098 };
static const rgb_t BORD  = { 0.20, 0.55, 0.58 };
static const rgb_t CYAN  = { 0.42, 0.82, 0.80 };
static const rgb_t GOLD  = { 0.83, 0.65, 0.36 };
static const rgb_t GREEN = { 0.45, 0.78, 0.45 };
static const rgb_t TEXT  = { 0.83,  0.87,  0.90 };
static const rgb_t DIM   = { 0.45,  0.50,  0.56 };

static void set_rgb(cairo_t *cr, rgb_t c)
{
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
}

static void text(cairo_t *cr, const char *s, double x, double y, rgb_t c, int bold)
{
    PangoLayout *L = pango_cairo_create_layout(cr);
    char d[128];
    snprintf(d, sizeof(d),  "%s%s %.0f", FONT, bold ? " Bold" : "", FSZ);
    PangoFontDescription *fd = pango_font_description_from_string(d);
    pango_layout_set_font_description(L, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(L, s, -1);
    set_rgb(cr, c);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, L);
    g_object_unref(L);
}

static void card(cairo_t *cr, double x, double y, double w, double h)
{
    set_rgb(cr, PANEL);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    set_rgb(cr, BORD);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);
}

static void bar(cairo_t *cr, double x, double y, double w, double h, double pct, rgb_t c)
{
    if (pct < 0) pct =  0.0;
    if (pct > 100) pct =  100.0;
    set_rgb(cr, PANEL);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    if (pct > 0 && w > 0)
    {
        double fw = w * (pct / 100.0);
        set_rgb(cr, c);
        cairo_rectangle(cr, x, y, fw, h);
        cairo_fill(cr);
    }
    set_rgb(cr, BORD);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);
}

static void fmt_kb(char *b, size_t z, unsigned long long k)
{
    if (k >= (1024ull * 1024ull))
    {
        snprintf(b, z, "%.2f GiB", (double)k / (1024.0 * 1024.0));
    }
    else if (k >= 1024ull)
    {
        snprintf(b, z, "%.1f MiB", (double)k / 1024.0);
    }
    else
    {
        snprintf(b, z, "%llu KiB", k);
    }
}

void swlsysinfo_render_draw(swlsysinfo_render *r, const struct swlsysinfo_snapshot *snap)
{
    if (!r || !snap) return;
    cairo_t *cr = cairo_create(r->surface);

    set_rgb(cr, BG);
    cairo_paint(cr);

    double x = PAD, y = PAD;
    double card_w = (double)r->width - 2 * PAD;
    double card_h = LINE_H + 24;

    text(cr, "SWL OS - MONITOR DO SISTEMA", x, y, CYAN, 1);
    if (snap->sys.hostname[0])
    {
        text(cr, snap->sys.hostname, x, y + 18, DIM, 0);
    }
    y += HEADER_H;

    card(cr, x, y, card_w, card_h);
    double cx = x + 10, cy = y + 10;
    text(cr, "CPU", cx, cy, GOLD, 1);
    if (snap->cpu.usage_pct >= 0)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%%", snap->cpu.usage_pct);
        text(cr, buf, x + card_w - 50, cy, CYAN, 1);
    }
    bar(cr, cx, cy + 22, card_w - 20, 8, snap->cpu.usage_pct, CYAN);
    text(cr, "SISTEMA", cx, cy + 56, GOLD, 1);
    char la[64];
    snprintf(la, sizeof(la), "load: %.2f %.2f %.2f",
             snap->cpu.load1, snap->cpu.load5, snap->cpu.load15);
    text(cr, la, cx, cy + 74, TEXT, 0);
    y += card_h + 10;

    card(cr, x, y, card_w, card_h);
    cx = x + 10, cy = y + 10;
    text(cr, "MEMORIA", cx, cy, GOLD, 1);
    if (snap->mem.total_kb > 0)
    {
        char u[32], t[32];
        fmt_kb(u, sizeof(u), snap->mem.used_kb);
        fmt_kb(t, sizeof(t), snap->mem.total_kb);
        char ms[64];
        snprintf(ms, sizeof(ms), "%s / %s", u, t);
        text(cr, ms, x + card_w - 140, cy, CYAN, 1);
        double pc = snap->mem.total_kb > 0 ?
                 100.0 * (double)snap->mem.used_kb / (double)snap->mem.total_kb : 0.0;
        bar(cr, cx, cy + 22, card_w -  20, 8, pc, GREEN);
        if (snap->mem.swap_total_kb > 0)
        {
            char sw[64];
            snprintf(sw, sizeof(sw), "swap: %s/%s", u, t);
            text(cr, sw, cx, cy + 38, DIM, 0);
        }
        else
        {
            text(cr, "swap: --", cx, cy + 38, DIM, 0);
        }
    }
    else
    {
        text(cr, "(indisponivel)", cx, cy +  22, DIM, 0);
    }
    y += card_h + 10;

    card(cr, x, y, card_w, card_h);
    cx = x + 10, cy = y + 10;
    text(cr, "KERNEL", cx, cy, GOLD, 1);
    char kl[200];
    snprintf(kl, sizeof(kl), "%s - %d processos",
             snap->sys.kernel, snap->sys.process_count);
    text(cr, kl, cx, cy + 22, TEXT, 0);
    char up[32];
    snprintf(up, sizeof(up), "uptime: %s", snap->sys.uptime);
    text(cr, up, cx, cy + 42, DIM, 0);
    y += card_h + 10;

    double th = (double)r->height - y - PAD;
    if (th < 60) th =  60;
    card(cr, x, y, card_w, th);
    cx = x + 10, cy = y + 10;
    text(cr, "TOP PROCESSOS (CPU)", cx, cy, GOLD, 1);
    int shown = snap->nprocs < SWLSYSINFO_TOP_PROC ? snap->nprocs : SWLSYSINFO_TOP_PROC;
    double ty = cy + 24;
    for (int i = 0; i < shown; i++)
    {
        const struct swlsysinfo_proc *p = &snap->procs[i];
        char ln[96];
        if (p->cpu_pct >= 0) snprintf(ln, sizeof(ln), "%3.0f%%", p->cpu_pct);
        else snprintf(ln, sizeof(ln), "  --  ");
        snprintf(ln + strlen(ln), sizeof(ln) - strlen(ln), " %s", p->comm);
        text(cr, ln, cx, ty, i ==  0 ? CYAN : TEXT, 0);
        ty += LINE_H;
    }
    if (shown == 0) text(cr, "(sem dados)", cx, ty, DIM, 0);

    cairo_destroy(cr);
    cairo_surface_flush(r->surface);
}

swlsysinfo_render *swlsysinfo_render_new(int width, int height)
{
    swlsysinfo_render *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    r->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(r->surface) != CAIRO_STATUS_SUCCESS)
    {
        free(r);
        return NULL;
    }
    r->width = width;
    r->height = height;
    return r;
}

void swlsysinfo_render_free(swlsysinfo_render *r)
{
    if (!r) return;
    cairo_surface_destroy(r->surface);
    free(r);
}

cairo_surface_t *swlsysinfo_render_surface(swlsysinfo_render *r)
{
    return r->surface;
}
