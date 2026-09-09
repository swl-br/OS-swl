#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "taskbar.h"
#include "desktop.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#define SWL_TASKBAR_TICK_MS 1000
#define SWL_TASKBAR_MENU_W  90
#define SWL_TASKBAR_WIN_W   170
#define SWL_TASKBAR_GAP     6
#define SWL_TASKBAR_ICON_SZ 16

/* --- ícones da bandeja do sistema (vetoriais, tamanho fixo ~16px) ---
 * Puramente decorativos por enquanto — não tem fonte real de volume/wifi/
 * bateria ligada ainda (não existe backend de áudio/rede/energia no
 * projeto). Mesma natureza do que já era (texto ")))" / "NET" também
 * era só decorativo) — só troca a forma de mostrar isso. */
static void icon_volume(cairo_t *cr, double x, double y, double s, int percent) {
	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_set_line_width(cr, 1.3);
	/* corpo do alto-falante (trapézio) */
	cairo_move_to(cr, 0, s * 0.35);
	cairo_line_to(cr, s * 0.30, s * 0.35);
	cairo_line_to(cr, s * 0.55, s * 0.10);
	cairo_line_to(cr, s * 0.55, s * 0.90);
	cairo_line_to(cr, s * 0.30, s * 0.65);
	cairo_line_to(cr, 0, s * 0.65);
	cairo_close_path(cr);
	cairo_stroke(cr);
	if (percent < 0) {
		/* Sem leitura (amixer ausente ou sem placa de som — comum em VM/
		 * container): um traço diagonal simples avisa "sem info", em vez
		 * de fingir uma onda de som que não corresponde a nada real. */
		cairo_move_to(cr, s * 0.05, s * 0.05);
		cairo_line_to(cr, s * 0.75, s * 0.75);
		cairo_stroke(cr);
	} else if (percent == 0) {
		/* Mudo: sem onda nenhuma. */
	} else {
		/* Onda de som — uma curva se volume baixo/médio, duas se alto. */
		cairo_arc(cr, s * 0.30, s * 0.5, s * 0.42, -M_PI / 4, M_PI / 4);
		cairo_stroke(cr);
		if (percent > 60) {
			cairo_arc(cr, s * 0.30, s * 0.5, s * 0.62, -M_PI / 5, M_PI / 5);
			cairo_stroke(cr);
		}
	}
	cairo_restore(cr);
}

static void icon_wifi(cairo_t *cr, double x, double y, double s, int percent) {
	cairo_save(cr);
	cairo_translate(cr, x, y + s * 0.9);
	cairo_set_line_width(cr, 1.3);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	/* 3 barrinhas, cada uma "acende" (cor normal) se o sinal alcança o
	 * limiar dela; senão fica esmaecida (SWL_COL_PANEL_BORDER). Sem
	 * interface wifi (percent < 0): as três ficam esmaecidas. */
	int thresholds[3] = { 1, 34, 67 };
	for (int i = 0; i < 3; i++) {
		double bw = s * 0.16;
		double bh = s * (0.28 + i * 0.30);
		double bx = i * (bw + s * 0.08);
		bool lit = percent >= thresholds[i];
		SWL_SET(cr, lit ? SWL_COL_TEXT_DIM : SWL_COL_PANEL_BORDER);
		cairo_move_to(cr, bx, 0);
		cairo_line_to(cr, bx, -bh);
		cairo_stroke(cr);
	}
	cairo_restore(cr);
}

static void icon_battery(cairo_t *cr, double x, double y, double s, double frac) {
	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_set_line_width(cr, 1.2);
	double w = s * 0.9, h = s * 0.55, top = (s - h) / 2.0;
	cairo_rectangle(cr, 0, top, w, h);
	cairo_stroke(cr);
	/* terminal (+) do lado direito */
	cairo_rectangle(cr, w, top + h * 0.25, s * 0.08, h * 0.5);
	cairo_fill(cr);
	/* nível — real (lido de /sys/class/power_supply), não mais fixo.
	 * frac < 0 (sem bateria detectada — comum em desktop/VM): não
	 * preenche nada, só o contorno, deixando claro que não tem leitura
	 * em vez de mentir uma porcentagem qualquer. */
	if (frac >= 0) {
		if (frac > 1) frac = 1;
		cairo_rectangle(cr, s * 0.08, top + s * 0.08,
			(w - s * 0.16) * frac, h - s * 0.16);
		cairo_fill(cr);
	}
	cairo_restore(cr);
}

/* --- leituras reais da bandeja (bateria/wifi/volume) -----------------
 * Mesmo espírito das leituras de CPU/MEM do painel: direto de /proc e
 * /sys, sem linkar biblioteca nenhuma — exceto o volume, que não tem
 * uma leitura de "porcentagem atual" confiável só em sysfs; usa o
 * utilitário `amixer` (pacote alsa-utils) via popen, com fallback
 * gracioso se não existir ou não tiver placa de som (comum neste
 * ambiente de dev em container, e possivelmente no SWL OS mínimo
 * também, até decidirem empacotar alsa-utils no rootfs — ver ressalva
 * na entrega). Todas as três retornam -1 quando a informação não está
 * disponível — quem desenha decide o que mostrar nesse caso. */

static int read_battery_percent(bool *out_charging) {
	static const char *bases[] = {
		"/sys/class/power_supply/BAT0",
		"/sys/class/power_supply/BAT1",
	};
	for (size_t i = 0; i < sizeof(bases) / sizeof(bases[0]); i++) {
		char path[128];
		snprintf(path, sizeof(path), "%s/capacity", bases[i]);
		FILE *f = fopen(path, "r");
		if (!f) {
			continue;
		}
		int pct = -1;
		if (fscanf(f, "%d", &pct) != 1) {
			pct = -1;
		}
		fclose(f);
		if (pct < 0) {
			continue;
		}
		if (out_charging) {
			*out_charging = false;
			snprintf(path, sizeof(path), "%s/status", bases[i]);
			FILE *fs = fopen(path, "r");
			if (fs) {
				char status[32] = {0};
				if (fgets(status, sizeof(status), fs)) {
					*out_charging = (strncmp(status, "Charging", 8) == 0);
				}
				fclose(fs);
			}
		}
		return pct;
	}
	return -1; /* sem bateria (desktop/VM) — resultado válido, não é erro */
}

static int read_wifi_percent(void) {
	FILE *f = fopen("/proc/net/wireless", "r");
	if (!f) {
		return -1;
	}
	char line[256];
	int result = -1;
	/* 2 linhas de cabeçalho, depois uma linha por interface —
	 * "wlan0: 0000   NN.  ..." onde o 2º campo é a qualidade do link.
	 * A escala varia por driver (comum: 0-70), normaliza pra 0-100. */
	for (int i = 0; i < 2 && fgets(line, sizeof(line), f); i++) {
		/* descarta cabeçalho */
	}
	if (fgets(line, sizeof(line), f)) {
		char iface[32];
		unsigned status;
		double quality;
		if (sscanf(line, " %31[^:]: %x %lf", iface, &status, &quality) == 3) {
			double pct = (quality / 70.0) * 100.0;
			if (pct > 100) pct = 100;
			if (pct < 0) pct = 0;
			result = (int)pct;
		}
	}
	fclose(f);
	return result;
}

static int read_volume_percent(void) {
	FILE *f = popen("amixer get Master 2>/dev/null", "r");
	if (!f) {
		return -1;
	}
	char line[256];
	int result = -1;
	while (fgets(line, sizeof(line), f)) {
		char *bracket = strchr(line, '[');
		if (bracket && strchr(bracket, '%')) {
			int pct;
			if (sscanf(bracket, "[%d%%]", &pct) == 1) {
				result = pct;
				break;
			}
		}
	}
	pclose(f);
	return result;
}

static void taskbar_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_taskbar *tb = data;

	SWL_SET(cr, SWL_COL_PANEL_BG);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	SWL_SET(cr, SWL_COL_PANEL_BORDER);
	swl_draw_hline(cr, 0, 0, width, 1);

	double cy = height / 2.0 - 7;

	/* Botão MENU — mantido como estava (usuário confirmou que já tava bom
	 * nessa entrega, só contorno, sem preenchimento). */
	SWL_SET(cr, SWL_COL_ACCENT_PURPLE);
	cairo_set_line_width(cr, 1);
	cairo_rectangle(cr, 6, 4, SWL_TASKBAR_MENU_W - 12, height - 8);
	cairo_stroke(cr);
	swl_draw_text(cr, "MENU", 22, cy, 11, SWL_FONT_MONO, true);

	/* Botões das janelas abertas — sem caixa completa em cada um (isso
	 * era mais "genérico"/pesado visualmente); só uma linha divisória
	 * fina entre eles, e a janela focada ganha um traço embaixo em vez
	 * de um contorno inteiro — mais parecido com a referência e menos
	 * poluído com muitas janelas abertas. */
	double x = SWL_TASKBAR_MENU_W + 10;
	for (int i = 0; i < tb->count && i < SWL_TASKBAR_MAX_WINDOWS; i++) {
		double w = SWL_TASKBAR_WIN_W;
		bool focused = (i == tb->focused);

		if (i > 0) {
			SWL_SET(cr, SWL_COL_PANEL_BORDER);
			cairo_set_line_width(cr, 1);
			cairo_move_to(cr, x - SWL_TASKBAR_GAP / 2.0, 6);
			cairo_line_to(cr, x - SWL_TASKBAR_GAP / 2.0, height - 6);
			cairo_stroke(cr);
		}
		if (focused) {
			SWL_SET(cr, SWL_COL_ACCENT_CYAN);
			cairo_set_line_width(cr, 2);
			cairo_move_to(cr, x + 2, height - 3);
			cairo_line_to(cr, x + w - 2, height - 3);
			cairo_stroke(cr);
		}

		const char *title = tb->titles[i] ? tb->titles[i] : "janela";
		swl_desktop_draw_glyph_for_title(cr, title,
			x + 8, height / 2.0 - SWL_TASKBAR_ICON_SZ / 2.0, SWL_TASKBAR_ICON_SZ);

		SWL_SET(cr, focused ? SWL_COL_TEXT : SWL_COL_TEXT_DIM);
		char label[48];
		snprintf(label, sizeof(label), "%.20s", title);
		swl_draw_text(cr, label, x + 8 + SWL_TASKBAR_ICON_SZ + 6, cy, 10, SWL_FONT_MONO, false);

		tb->win_btn_x[i] = (int)x;
		tb->win_btn_w[i] = (int)w;

		x += w + SWL_TASKBAR_GAP;
	}

	/* Bandeja do sistema — volume via amixer (best-effort, ver nota
	 * acima), wifi/bateria direto de /proc e /sys. Lido a cada desenho
	 * (chamado no máximo 1x/segundo pelo timer da taskbar — não é caro
	 * o bastante pra precisar de cache). */
	double tray_x = width - 230;
	if (tray_x < x + 20) {
		tray_x = x + 20;
	}
	SWL_SET(cr, SWL_COL_TEXT_DIM);
	double tray_icon_y = height / 2.0 - 8;
	int vol_pct = read_volume_percent();
	int wifi_pct = read_wifi_percent();
	bool charging = false;
	int bat_pct = read_battery_percent(&charging);
	icon_volume(cr, tray_x, tray_icon_y, 16, vol_pct);
	icon_wifi(cr, tray_x + 30, tray_icon_y, 16, wifi_pct);
	SWL_SET(cr, charging ? SWL_COL_ACCENT_CYAN : SWL_COL_TEXT_DIM);
	icon_battery(cr, tray_x + 60, tray_icon_y, 16,
		bat_pct < 0 ? -1.0 : bat_pct / 100.0);

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
