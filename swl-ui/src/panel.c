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

static void draw_sparkline(cairo_t *cr, double x, double y, double w, double h,
		const double *history, int len, swl_color_t color) {
	SWL_SET(cr, SWL_COL_TEXT_DIM);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, x, y, w, h);
	cairo_stroke(cr);

	if (len <= 0) {
		return;
	}
	double bar_w = (w - 2) / (double)len;
	SWL_SET(cr, color);
	for (int i = 0; i < len; i++) {
		double frac = history[i];
		if (frac < 0) frac = 0;
		if (frac > 1) frac = 1;
		double bh = (h - 2) * frac;
		if (bh < 1 && frac > 0) {
			bh = 1; /* pelo menos 1px visível — senão uso baixo "some" */
		}
		double bx = x + 1 + i * bar_w;
		double bw = bar_w - 1;
		if (bw < 1) bw = 1;
		double by = y + h - 1 - bh;
		cairo_rectangle(cr, bx, by, bw, bh);
		cairo_fill(cr);
	}
}

/* Empurra uma nova amostra no histórico (mais antiga sai pela esquerda,
 * a nova entra no final) — mesmo efeito visual de um sparkline "andando"
 * a cada tick, como o equalizador da referência visual do projeto. */
static void push_history(double *history, int len, double value) {
	memmove(history, history + 1, (len - 1) * sizeof(double));
	history[len - 1] = value;
}

static void panel_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_panel *panel = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	SWL_SET(cr, SWL_COL_PANEL_BORDER);
	swl_draw_hline(cr, 0, height - 1, width, 1);

	double cy = height / 2.0 - 7;

	/* Logo + versão + arquitetura — texto claro simples (o destaque de
	 * cor fica pro menu, não pro título, batendo com a referência
	 * visual: "SWL OS v1.0.0  x86^32bit" sem coloração especial). */
	SWL_SET(cr, SWL_COL_TEXT);
	swl_draw_text(cr, "SWL OS", 12, cy, 11, SWL_FONT_MONO, true);
	SWL_SET(cr, SWL_COL_TEXT_DIM);
	swl_draw_text(cr, "v1.0.0", 76, cy, 11, SWL_FONT_MONO, false);
	swl_draw_text(cr, "x86 32bit", 136, cy, 11, SWL_FONT_MONO, false);

	/* Menu — espaçado entre letras (visual "tracked out" retrô) e na cor
	 * de destaque, em vez do título (referência visual: menu em ciano,
	 * título em branco simples — inverso do que tínhamos antes). Clique
	 * ainda só funciona pra SISTEMA (abre "Opções do painel"); os
	 * outros continuam só visuais por enquanto. */
	static const char *menu[] = {
		"ARQUIVO", "EDITAR", "EXIBIR", "FERRAMENTAS", "SISTEMA", "AJUDA"
	};
	#define SWL_PANEL_TRACKING 1.0
	double mx = 235;
	panel->sistema_x = 0;
	panel->sistema_w = 0;
	for (size_t i = 0; i < sizeof(menu) / sizeof(menu[0]); i++) {
		double w, h;
		swl_text_extents_tracked(menu[i], 11, SWL_FONT_MONO, false,
			SWL_PANEL_TRACKING, &w, &h);
		SWL_SET(cr, SWL_COL_ACCENT_CYAN);
		swl_draw_text_tracked(cr, menu[i], mx, cy, 11, SWL_FONT_MONO, false,
			SWL_PANEL_TRACKING);
		if (strcmp(menu[i], "SISTEMA") == 0) {
			panel->sistema_x = (int)mx - 6;
			panel->sistema_w = (int)w + 12;
		}
		mx += w + 14;
	}

	/* Bloco direito: CPU, MEM, data/hora — montado da direita pra esquerda,
	 * medindo a largura real de cada texto antes de desenhar qualquer
	 * coisa. O layout antigo usava um offset fixo (width - 300) pra
	 * decidir onde tudo começava, sem levar em conta que "MEM
	 * 1234M/5678M" + a data/hora completa já passam disso sozinhos —
	 * resultado: os dois textos caindo um em cima do outro em janelas
	 * menores (o "03" da data grudado no "MEM"). Aqui cada peça mede seu
	 * próprio espaço e só desenha se couber sem sobrepor o menu; do
	 * contrário some (nunca sobrepõe) até a janela ter espaço de novo. */
	const double GAP = 10;
	const double BAR_W = 40;

	time_t now = time(NULL);
	struct tm tm_info;
	localtime_r(&now, &tm_info);
	char datetime[32];
	strftime(datetime, sizeof(datetime),
		panel->clock_show_seconds ? "%d/%m/%Y %H:%M:%S" : "%d/%m/%Y %H:%M",
		&tm_info);
	double dw, dh;
	swl_text_extents(datetime, 11, SWL_FONT_MONO, false, &dw, &dh);

	long used_mb = 0, total_mb = 0;
	read_mem_usage(&used_mb, &total_mb);
	double mem_frac = total_mb > 0 ? (double)used_mb / (double)total_mb : 0;
	push_history(panel->mem_history, SWL_PANEL_HISTORY_LEN, mem_frac);
	char mem_label[40];
	snprintf(mem_label, sizeof(mem_label), "MEM %ldM/%ldM", used_mb, total_mb);
	double mw, mh;
	swl_text_extents(mem_label, 11, SWL_FONT_MONO, false, &mw, &mh);

	double cpu_percent = 0;
	read_cpu_usage(&panel->prev_idle, &panel->prev_total, &cpu_percent);
	push_history(panel->cpu_history, SWL_PANEL_HISTORY_LEN, cpu_percent / 100.0);
	char cpu_label[16];
	snprintf(cpu_label, sizeof(cpu_label), "CPU %02.0f%%", cpu_percent);
	double cw, ch;
	swl_text_extents(cpu_label, 11, SWL_FONT_MONO, false, &cw, &ch);

	/* Cada bloco é [rótulo][barra], igual à referência visual ("CPU 07%"
	 * seguido da barra, não o contrário). Monta da direita pra esquerda:
	 * data/hora primeiro, depois bloco MEM, depois bloco CPU — cada um
	 * só entra na conta se estiver habilitado (show_cpu/show_mem). */
	double x_datetime = width - 12 - dw;
	double cursor_x = x_datetime;

	double x_mem = 0, x_mem_bar = 0;
	if (panel->show_mem) {
		cursor_x -= GAP;
		double mem_bar_end = cursor_x;
		x_mem_bar = mem_bar_end - BAR_W;
		x_mem = x_mem_bar - 8 - mw;
		cursor_x = x_mem;
	}

	double x_cpu = 0, x_cpu_bar = 0;
	if (panel->show_cpu) {
		cursor_x -= GAP;
		double cpu_bar_end = cursor_x;
		x_cpu_bar = cpu_bar_end - BAR_W;
		x_cpu = x_cpu_bar - 8 - cw;
		cursor_x = x_cpu;
	}

	double leftmost = cursor_x;

	if (leftmost >= mx + 14) {
		if (panel->show_cpu) {
			SWL_SET(cr, SWL_COL_TEXT);
			swl_draw_text(cr, cpu_label, x_cpu, cy, 11, SWL_FONT_MONO, false);
			draw_sparkline(cr, x_cpu_bar, cy - 3, BAR_W, 15,
				panel->cpu_history, SWL_PANEL_HISTORY_LEN, SWL_COL_ACCENT_CYAN);
		}
		if (panel->show_mem) {
			SWL_SET(cr, SWL_COL_TEXT);
			swl_draw_text(cr, mem_label, x_mem, cy, 11, SWL_FONT_MONO, false);
			draw_sparkline(cr, x_mem_bar, cy - 3, BAR_W, 15,
				panel->mem_history, SWL_PANEL_HISTORY_LEN, SWL_COL_ACCENT_PURPLE);
		}
		SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
		swl_draw_text(cr, datetime, x_datetime, cy, 11, SWL_FONT_MONO, false);
	} else {
		/* Janela estreita demais: mostra só a data/hora, sem CPU/MEM. */
		SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
		swl_draw_text(cr, datetime, x_datetime, cy, 11, SWL_FONT_MONO, false);
	}
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
	/* calloc zera tudo — mas queremos CPU/MEM/segundos visíveis por
	 * padrão, então liga explicitamente (mesmo comportamento visual de
	 * antes do popup de opções existir). */
	panel->show_cpu = true;
	panel->show_mem = true;
	panel->clock_show_seconds = true;
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

bool swl_panel_hit_test_sistema(struct swl_panel *panel, double x, double y) {
	if (y < 0 || y >= SWL_PANEL_HEIGHT) {
		return false;
	}
	return x >= panel->sistema_x && x < panel->sistema_x + panel->sistema_w;
}

void swl_panel_redraw_now(struct swl_panel *panel) {
	swl_buffer_redraw(panel->buffer, panel->width, SWL_PANEL_HEIGHT, panel_draw, panel);
}

void swl_panel_destroy(struct swl_panel *panel) {
	if (!panel) {
		return;
	}
	wl_event_source_remove(panel->timer);
	wlr_scene_node_destroy(&panel->tree->node);
	free(panel);
}
