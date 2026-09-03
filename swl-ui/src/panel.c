#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "panel.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define SWL_PANEL_TICK_MS 1000

/* Lê /proc/stat e calcula % de uso de CPU pelo delta desde a última leitura.
 * Não aloca nada, não depende de libs externas — leve o suficiente pra
 * rodar a cada segundo mesmo em hardware bem modesto. */
static void read_cpu_usage(unsigned long long *prev_idle,
		unsigned long long *prev_total, double *out_percent) {
	FILE *f = fopen("/proc/stat", "r");
	if (!f) {
		return;
	}
	char label[16];
	unsigned long long user, nice_, system_, idle, iowait, irq, softirq, steal;
	int n = fscanf(f, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
		label, &user, &nice_, &system_, &idle, &iowait, &irq, &softirq, &steal);
	fclose(f);
	if (n < 5) {
		return;
	}

	unsigned long long idle_all = idle + iowait;
	unsigned long long total = user + nice_ + system_ + idle_all + irq + softirq + steal;

	if (*prev_total != 0 && total > *prev_total) {
		unsigned long long delta_total = total - *prev_total;
		unsigned long long delta_idle = idle_all - *prev_idle;
		if (delta_total > 0) {
			*out_percent = 100.0 * (double)(delta_total - delta_idle) / (double)delta_total;
		}
	}
	*prev_idle = idle_all;
	*prev_total = total;
}

static bool read_mem_usage(long *used_mb, long *total_mb) {
	FILE *f = fopen("/proc/meminfo", "r");
	if (!f) {
		return false;
	}
	long mem_total = 0, mem_available = 0;
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		long val;
		if (sscanf(line, "MemTotal: %ld kB", &val) == 1) {
			mem_total = val;
		} else if (sscanf(line, "MemAvailable: %ld kB", &val) == 1) {
			mem_available = val;
		}
	}
	fclose(f);
	if (mem_total == 0) {
		return false;
	}
	*total_mb = mem_total / 1024;
	*used_mb = (mem_total - mem_available) / 1024;
	return true;
}

static void draw_bar(cairo_t *cr, double x, double y, double w, double h, double frac) {
	if (frac < 0) frac = 0;
	if (frac > 1) frac = 1;

	SWL_SET(cr, SWL_COL_TEXT_DIM);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, x, y, w, h);
	cairo_stroke(cr);

	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	cairo_rectangle(cr, x + 1, y + 1, (w - 2) * frac, h - 2);
	cairo_fill(cr);
}

static void panel_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_panel *panel = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	SWL_SET(cr, SWL_COL_PANEL_BORDER);
	swl_draw_hline(cr, 0, height - 1, width, 1);

	double cy = height / 2.0 - 7;

	/* Logo + versão + arquitetura, como no mockup */
	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	swl_draw_text(cr, "SWL OS", 12, cy, 11, SWL_FONT_MONO, true);
	SWL_SET(cr, SWL_COL_TEXT_DIM);
	swl_draw_text(cr, "v1.0.0", 76, cy, 11, SWL_FONT_MONO, false);
	swl_draw_text(cr, "x86 32bit", 136, cy, 11, SWL_FONT_MONO, false);

	/* Menu (visual por enquanto — clique nos itens ainda não abre nada) */
	static const char *menu[] = {
		"ARQUIVO", "EDITAR", "EXIBIR", "FERRAMENTAS", "SISTEMA", "AJUDA"
	};
	double mx = 260;
	for (size_t i = 0; i < sizeof(menu) / sizeof(menu[0]); i++) {
		SWL_SET(cr, SWL_COL_TEXT);
		swl_draw_text(cr, menu[i], mx, cy, 11, SWL_FONT_MONO, false);
		double w, h;
		swl_text_extents(menu[i], 11, SWL_FONT_MONO, false, &w, &h);
		mx += w + 24;
	}

	/* Bloco direito: CPU, MEM, data/hora */
	double rx = width - 300;
	if (rx < mx + 20) {
		rx = mx + 20; /* evita sobrepor o menu em telas muito estreitas */
	}

	double cpu_percent = 0;
	read_cpu_usage(&panel->prev_idle, &panel->prev_total, &cpu_percent);
	char cpu_label[16];
	snprintf(cpu_label, sizeof(cpu_label), "CPU %02.0f%%", cpu_percent);
	SWL_SET(cr, SWL_COL_TEXT);
	swl_draw_text(cr, cpu_label, rx, cy, 11, SWL_FONT_MONO, false);
	draw_bar(cr, rx + 62, cy + 2, 60, 9, cpu_percent / 100.0);

	long used_mb = 0, total_mb = 0;
	read_mem_usage(&used_mb, &total_mb);
	char mem_label[40];
	snprintf(mem_label, sizeof(mem_label), "MEM %ldM/%ldM", used_mb, total_mb);
	SWL_SET(cr, SWL_COL_TEXT);
	swl_draw_text(cr, mem_label, rx + 140, cy, 11, SWL_FONT_MONO, false);

	time_t now = time(NULL);
	struct tm tm_info;
	localtime_r(&now, &tm_info);
	char datetime[32];
	strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", &tm_info);
	double dw, dh;
	swl_text_extents(datetime, 11, SWL_FONT_MONO, false, &dw, &dh);
	SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
	swl_draw_text(cr, datetime, width - dw - 12, cy, 11, SWL_FONT_MONO, false);
}

static int panel_tick(void *data) {
	struct swl_panel *panel = data;
	swl_buffer_redraw(panel->buffer, panel->width, SWL_PANEL_HEIGHT, panel_draw, panel);
	wl_event_source_timer_update(panel->timer, SWL_PANEL_TICK_MS);
	return 0;
}

struct swl_panel *swl_panel_create(struct wl_event_loop *loop,
		struct wlr_scene_tree *parent, int width) {
	struct swl_panel *panel = calloc(1, sizeof(*panel));
	panel->width = width;
	panel->tree = wlr_scene_tree_create(parent);
	panel->buffer = swl_buffer_create(panel->tree, width, SWL_PANEL_HEIGHT, panel_draw, panel);
	wlr_scene_node_set_position(&panel->tree->node, 0, 0);
	panel->timer = wl_event_loop_add_timer(loop, panel_tick, panel);
	wl_event_source_timer_update(panel->timer, SWL_PANEL_TICK_MS);
	return panel;
}

void swl_panel_resize(struct swl_panel *panel, int width) {
	panel->width = width;
	swl_buffer_redraw(panel->buffer, width, SWL_PANEL_HEIGHT, panel_draw, panel);
}

void swl_panel_destroy(struct swl_panel *panel) {
	if (!panel) {
		return;
	}
	wl_event_source_remove(panel->timer);
	wlr_scene_node_destroy(&panel->tree->node);
	free(panel);
}
