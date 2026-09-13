#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "decorations.h"
#include "swl_buffer.h"
#include "swl_draw_util.h"
#include "theme.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Padrão visual novo (decisão de arquitetura tomada em conjunto):
 * barra de título com cantos superiores arredondados, título
 * centralizado, três botões CIRCULARES no canto esquerdo (fechar,
 * minimizar, maximizar — nessa ordem, igual macOS mas com as CORES do
 * SWL: vermelho, âmbar, roxo). Fundo plano (sem gradiente/vidro) que
 * acompanha o tema ativo (claro/escuro).
 *
 * Quem desenha o CONTEÚDO da janela é o app (via swlappkit); isto aqui
 * é só a moldura, desenhada pelo compositor — ver decisão registrada
 * em docs/ai/DECISIONS.md.
 */

#define BTN_R       6.0
#define BTN_GAP     8.0
#define BTN_MARGIN  10.0
#define DECO_RADIUS 10.0

/* Lê o tema ativo (claro/escuro) de um arquivo simples e compartilhado
 * entre compositor e apps — ainda não existe um barramento (swl-bus)
 * pra isso, então por enquanto é um arquivo texto com "light" ou
 * "dark" na primeira linha. Falha ao ler = escuro (padrão do sistema). */
static bool theme_is_light(void) {
	const char *home = getenv("HOME");
	if (!home || !*home) {
		return false;
	}
	char path[512];
	snprintf(path, sizeof(path), "%s/.config/swl/theme", home);
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	char line[16] = {0};
	if (!fgets(line, sizeof(line), f)) {
		fclose(f);
		return false;
	}
	fclose(f);
	return strncmp(line, "light", 5) == 0;
}

/* Path de retângulo só com os cantos de CIMA arredondados — embaixo
 * fica reto porque a barra emenda direto no conteúdo da janela. */
static void top_rounded_rect_path(cairo_t *cr, double w, double h, double r) {
	double max_r = (w < h * 2.0 ? w / 2.0 : h);
	if (r > max_r) {
		r = max_r;
	}
	cairo_new_sub_path(cr);
	cairo_move_to(cr, 0, h);
	cairo_line_to(cr, 0, r);
	cairo_arc(cr, r, r, r, M_PI, 3.0 * M_PI / 2.0);
	cairo_line_to(cr, w - r, 0);
	cairo_arc(cr, w - r, r, r, 3.0 * M_PI / 2.0, 2.0 * M_PI);
	cairo_line_to(cr, w, h);
	cairo_close_path(cr);
}

static void draw_traffic_light(cairo_t *cr, double cx, double cy, double r,
		swl_color_t color, bool focused, bool light) {
	/* sem foco: bolinha "apagada" — evita todo mundo gritando cor na
	 * tela ao mesmo tempo quando há várias janelas abertas. */
	swl_color_t c = color;
	if (!focused) {
		c = light ? SWL_COL_TEXT_DIM_LIGHT : SWL_COL_TEXT_DIM;
	}
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	SWL_SET(cr, c);
	cairo_fill(cr);
}

static void decoration_draw(cairo_t *cr, int width, int height, void *data) {
	struct swl_decoration *deco = data;
	bool light = theme_is_light();

	swl_color_t bg = light ? SWL_COL_PANEL_BG_LIGHT : SWL_COL_PANEL_BG;
	swl_color_t border = light ? SWL_COL_PANEL_BORDER_LIGHT : SWL_COL_PANEL_BORDER;
	swl_color_t accent_border = deco->focused ? SWL_COL_ACCENT_CYAN : border;
	swl_color_t text = light ? SWL_COL_TEXT_LIGHT : SWL_COL_TEXT;
	swl_color_t text_dim = light ? SWL_COL_TEXT_DIM_LIGHT : SWL_COL_TEXT_DIM;

	/* fundo com cantos superiores arredondados */
	top_rounded_rect_path(cr, width, height, DECO_RADIUS);
	SWL_SET(cr, bg);
	cairo_fill_preserve(cr);
	SWL_SET(cr, accent_border);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);

	/* título centralizado */
	const char *title = deco->title ? deco->title : "janela";
	double tw = 0, th = 0;
	swl_text_extents(title, 10, SWL_FONT_MONO, false, &tw, &th);
	SWL_SET(cr, deco->focused ? text : text_dim);
	swl_draw_text(cr, title, (width - tw) / 2.0, height / 2.0 - th / 2.0,
		10, SWL_FONT_MONO, false);

	/* três bolinhas à esquerda: fechar, minimizar, maximizar — nessa
	 * ordem (convenção conhecida, só recolorida pro nosso padrão). */
	double cx = BTN_MARGIN + BTN_R;
	double cy = height / 2.0;
	draw_traffic_light(cr, cx, cy, BTN_R, SWL_COL_DANGER, deco->focused, light);
	cx += 2.0 * BTN_R + BTN_GAP;
	draw_traffic_light(cr, cx, cy, BTN_R, SWL_COL_WARN, deco->focused, light);
	cx += 2.0 * BTN_R + BTN_GAP;
	draw_traffic_light(cr, cx, cy, BTN_R, SWL_COL_ACCENT_PURPLE, deco->focused, light);
}

struct swl_decoration *swl_decoration_create(struct wlr_scene_tree *parent,
		int width, const char *title) {
	struct swl_decoration *deco = calloc(1, sizeof(*deco));
	deco->width = width;
	deco->title = strdup(title ? title : "janela");
	deco->focused = true;
	deco->tree = wlr_scene_tree_create(parent);
	deco->buffer = swl_buffer_create(deco->tree, width, SWL_TITLEBAR_HEIGHT,
		decoration_draw, deco);
	wlr_scene_node_set_position(&deco->tree->node, 0, 0);
	return deco;
}

void swl_decoration_set_title(struct swl_decoration *deco, const char *title) {
	free(deco->title);
	deco->title = strdup(title ? title : "janela");
	swl_buffer_redraw(deco->buffer, deco->width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

void swl_decoration_set_focused(struct swl_decoration *deco, bool focused) {
	if (deco->focused == focused) {
		return;
	}
	deco->focused = focused;
	swl_buffer_redraw(deco->buffer, deco->width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

void swl_decoration_resize(struct swl_decoration *deco, int width) {
	if (width == deco->width || width <= 0) {
		return;
	}
	deco->width = width;
	swl_buffer_redraw(deco->buffer, width, SWL_TITLEBAR_HEIGHT, decoration_draw, deco);
}

enum swl_deco_button swl_decoration_hit_test(struct swl_decoration *deco,
		double local_x, double local_y) {
	if (local_y < 0 || local_y >= SWL_TITLEBAR_HEIGHT ||
			local_x < 0 || local_x >= deco->width) {
		return SWL_DECO_NONE;
	}

	/* hit-box quadrado ao redor de cada bolinha (raio + folga) — clicar
	 * perto já ativa, não precisa acertar o pixel exato do círculo. */
	double hit_half = BTN_R + 3.0;
	double cy = SWL_TITLEBAR_HEIGHT / 2.0;
	double cx = BTN_MARGIN + BTN_R;

	if (local_x >= cx - hit_half && local_x < cx + hit_half &&
			local_y >= cy - hit_half && local_y < cy + hit_half) {
		return SWL_DECO_CLOSE;
	}
	cx += 2.0 * BTN_R + BTN_GAP;
	if (local_x >= cx - hit_half && local_x < cx + hit_half &&
			local_y >= cy - hit_half && local_y < cy + hit_half) {
		return SWL_DECO_MINIMIZE;
	}
	cx += 2.0 * BTN_R + BTN_GAP;
	if (local_x >= cx - hit_half && local_x < cx + hit_half &&
			local_y >= cy - hit_half && local_y < cy + hit_half) {
		return SWL_DECO_MAXIMIZE;
	}
	return SWL_DECO_DRAG;
}

void swl_decoration_destroy(struct swl_decoration *deco) {
	if (!deco) {
		return;
	}
	free(deco->title);
	wlr_scene_node_destroy(&deco->tree->node);
	free(deco);
}
