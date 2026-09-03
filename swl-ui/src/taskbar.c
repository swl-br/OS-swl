#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "taskbar.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define SWL_TASKBAR_TICK_MS 1000
#define SWL_TASKBAR_MENU_W  90
#define SWL_TASKBAR_WIN_W   170
#define SWL_TASKBAR_GAP     6

static void taskbar_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_taskbar *tb = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	SWL_SET(cr, SWL_COL_PANEL_BORDER);
	swl_draw_hline(cr, 0, 0, width, 1);

	double cy = height / 2.0 - 7;

	/* Botão MENU */
	SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, 6, 4, SWL_TASKBAR_MENU_W - 12, height - 8);
	cairo_stroke(cr);
	swl_draw_text(cr, "MENU", 22, cy, 11, SWL_FONT_MONO, true);

	/* Botões das janelas abertas */
	double x = SWL_TASKBAR_MENU_W + 10;
	for (int i = 0; i < tb->count && i < SWL_TASKBAR_MAX_WINDOWS; i++) {
		double w = SWL_TASKBAR_WIN_W;
		bool focused = (i == tb->focused);

		SWL_SET(cr, focused ? SWL_COL_ACCENT_CYAN : SWL_COL_TEXT_DIM);
		cairo_rectangle(cr, x, 4, w, height - 8);
		cairo_stroke(cr);

		SWL_SET(cr, focused ? SWL_COL_TEXT : SWL_COL_TEXT_DIM);
		char label[48];
		snprintf(label, sizeof(label), "%.20s", tb->titles[i] ? tb->titles[i] : "janela");
		swl_draw_text(cr, label, x + 10, cy, 10, SWL_FONT_MONO, false);

		tb->win_btn_x[i] = (int)x;
		tb->win_btn_w[i] = (int)w;

		x += w + SWL_TASKBAR_GAP;
	}

	/* Bandeja do sistema — placeholders visuais (volume/rede) */
	double tray_x = width - 230;
	if (tray_x < x + 20) {
		tray_x = x + 20;
	}
	SWL_SET(cr, SWL_COL_TEXT_DIM);
	swl_draw_text(cr, ")))", tray_x, cy, 10, SWL_FONT_MONO, false);
	swl_draw_text(cr, "NET", tray_x + 60, cy, 10, SWL_FONT_MONO, false);

	/* Relógio */
	time_t now = time(NULL);
	struct tm tm_info;
	localtime_r(&now, &tm_info);
	char clock_str[8];
	strftime(clock_str, sizeof(clock_str), "%H:%M", &tm_info);
	double cw, ch;
	swl_text_extents(clock_str, 12, SWL_FONT_MONO, true, &cw, &ch);
	SWL_SET(cr, SWL_COL_ACCENT_CYAN);
	swl_draw_text(cr, clock_str, width - cw - 14, cy - 1, 12, SWL_FONT_MONO, true);
}

static int taskbar_tick(void *data) {
	struct swl_taskbar *tb = data;
	swl_buffer_redraw(tb->buffer, tb->width, SWL_TASKBAR_HEIGHT, taskbar_draw, tb);
	wl_event_source_timer_update(tb->timer, SWL_TASKBAR_TICK_MS);
	return 0;
}

struct swl_taskbar *swl_taskbar_create(struct wl_event_loop *loop,
		struct wlr_scene_tree *parent, int width, int y) {
	struct swl_taskbar *tb = calloc(1, sizeof(*tb));
	tb->width = width;
	tb->y = y;
	tb->focused = -1;
	tb->tree = wlr_scene_tree_create(parent);
	tb->buffer = swl_buffer_create(tb->tree, width, SWL_TASKBAR_HEIGHT, taskbar_draw, tb);
	wlr_scene_node_set_position(&tb->tree->node, 0, y);
	tb->timer = wl_event_loop_add_timer(loop, taskbar_tick, tb);
	wl_event_source_timer_update(tb->timer, SWL_TASKBAR_TICK_MS);
	return tb;
}

void swl_taskbar_resize(struct swl_taskbar *tb, int width, int y) {
	tb->width = width;
	tb->y = y;
	wlr_scene_node_set_position(&tb->tree->node, 0, y);
	swl_buffer_redraw(tb->buffer, width, SWL_TASKBAR_HEIGHT, taskbar_draw, tb);
}

void swl_taskbar_set_windows(struct swl_taskbar *tb,
		const char **titles, int count, int focused_index) {
	for (int i = 0; i < tb->count; i++) {
		free(tb->titles[i]);
		tb->titles[i] = NULL;
	}
	tb->count = count > SWL_TASKBAR_MAX_WINDOWS ? SWL_TASKBAR_MAX_WINDOWS : count;
	for (int i = 0; i < tb->count; i++) {
		tb->titles[i] = strdup(titles[i] ? titles[i] : "janela");
	}
	tb->focused = focused_index;
	swl_buffer_redraw(tb->buffer, tb->width, SWL_TASKBAR_HEIGHT, taskbar_draw, tb);
}

int swl_taskbar_hit_test(struct swl_taskbar *tb, double x, double y) {
	if (y < tb->y || y >= tb->y + SWL_TASKBAR_HEIGHT) {
		return -2;
	}
	if (x >= 6 && x < SWL_TASKBAR_MENU_W - 6) {
		return -1;
	}
	for (int i = 0; i < tb->count; i++) {
		if (x >= tb->win_btn_x[i] && x < tb->win_btn_x[i] + tb->win_btn_w[i]) {
			return i;
		}
	}
	return -2;
}

void swl_taskbar_destroy(struct swl_taskbar *tb) {
	if (!tb) {
		return;
	}
	for (int i = 0; i < tb->count; i++) {
		free(tb->titles[i]);
	}
	wl_event_source_remove(tb->timer);
	wlr_scene_node_destroy(&tb->tree->node);
	free(tb);
}
