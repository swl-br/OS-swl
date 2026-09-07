/*
 * gen_cursors.c — desenha, em Cairo, os glifos de cada formato de cursor
 * do tema "swl" (visual hacker-retrô: seta clara com contorno ciano,
 * acentos em ciano/magenta). Gera um PNG por (formato, tamanho).
 *
 * Uso: ./gen_cursors <diretório de saída>
 *
 * Cada função de desenho trabalha num espaço lógico de 24x24 unidades;
 * o tamanho real do PNG (24 ou 48px) é obtido escalando o canvas antes
 * de desenhar, então o mesmo código serve pros dois tamanhos sem
 * duplicar geometria.
 *
 * Não faz parte do build do compositor — é rodado uma vez por quem
 * mexer no visual do cursor, pra regenerar os PNGs que o generate.sh
 * empacota com xcursorgen em swl-ui/assets/cursors/swl/cursors/.
 */
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Paleta (repetida aqui, não inclui theme.h, pra essa ferramenta não
 * depender do resto do swl-ui — só do cairo). Mantida em sincronia
 * manual com swl-ui/include/theme.h; se a paleta mudar lá, atualizar
 * aqui também. */
#define COL_LINE_R 0.83
#define COL_LINE_G 0.87
#define COL_LINE_B 0.90 /* SWL_COL_TEXT: contorno/preenchimento claro */

#define COL_ACCENT_R 0.42
#define COL_ACCENT_G 0.82
#define COL_ACCENT_B 0.80 /* SWL_COL_ACCENT_CYAN: contorno de destaque */

#define COL_DANGER_R 0.86
#define COL_DANGER_G 0.35
#define COL_DANGER_B 0.40 /* SWL_COL_DANGER: not-allowed */

typedef void (*draw_fn)(cairo_t *cr);
typedef struct {
	const char *name;
	draw_fn draw;
	double hot_x, hot_y; /* hotspot em espaço lógico 24x24 */
} cursor_def_t;

static void stroke_fill(cairo_t *cr, double lw) {
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, lw);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_fill(cr);
}

/* --- default (seta / left_ptr) ------------------------------------- */
static void draw_default(cairo_t *cr) {
	cairo_move_to(cr, 2, 2);
	cairo_line_to(cr, 2, 19);
	cairo_line_to(cr, 6.2, 15.3);
	cairo_line_to(cr, 8.7, 21.2);
	cairo_line_to(cr, 11.2, 20.1);
	cairo_line_to(cr, 8.8, 14.3);
	cairo_line_to(cr, 14, 14.1);
	cairo_close_path(cr);
	stroke_fill(cr, 1.3);
}

/* --- text (I-beam) --------------------------------------------------- */
static void draw_text(cairo_t *cr) {
	/* barra vertical com serifas em cima/embaixo */
	cairo_move_to(cr, 8, 3); cairo_line_to(cr, 16, 3);
	cairo_move_to(cr, 12, 3); cairo_line_to(cr, 12, 21);
	cairo_move_to(cr, 8, 21); cairo_line_to(cr, 16, 21);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, 2.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_set_line_width(cr, 1.1);
	cairo_stroke(cr);
}

/* --- pointer (mão estilizada) ---------------------------------------- */
static void draw_pointer(cairo_t *cr) {
	/* silhueta simplificada: dedo indicador + punho, geométrico */
	cairo_move_to(cr, 9, 2);
	cairo_line_to(cr, 9, 12);
	cairo_line_to(cr, 7, 11);
	cairo_line_to(cr, 5.5, 12.3);
	cairo_line_to(cr, 9.5, 21.5);
	cairo_line_to(cr, 19, 21.5);
	cairo_line_to(cr, 19, 12.5);
	cairo_curve_to(cr, 19, 10.5, 16.5, 10, 15.5, 11.3);
	cairo_line_to(cr, 15.5, 8.5);
	cairo_curve_to(cr, 15.5, 7, 13, 7, 13, 8.5);
	cairo_line_to(cr, 13, 7.2);
	cairo_curve_to(cr, 13, 5.7, 10.6, 5.7, 10.6, 7.2);
	cairo_line_to(cr, 10.6, 3.3);
	cairo_curve_to(cr, 10.6, 1.6, 9, 1.6, 9, 2);
	cairo_close_path(cr);
	stroke_fill(cr, 1.1);
}

/* --- grab / move (cruz de 4 pontas) ----------------------------------- */
static void draw_grab(cairo_t *cr) {
	double cx = 12, cy = 12;
	cairo_move_to(cr, cx, cy - 10); cairo_line_to(cr, cx - 3, cy - 7); cairo_move_to(cr, cx, cy - 10); cairo_line_to(cr, cx + 3, cy - 7);
	cairo_move_to(cr, cx, cy + 10); cairo_line_to(cr, cx - 3, cy + 7); cairo_move_to(cr, cx, cy + 10); cairo_line_to(cr, cx + 3, cy + 7);
	cairo_move_to(cr, cx - 10, cy); cairo_line_to(cr, cx - 7, cy - 3); cairo_move_to(cr, cx - 10, cy); cairo_line_to(cr, cx - 7, cy + 3);
	cairo_move_to(cr, cx + 10, cy); cairo_line_to(cr, cx + 7, cy - 3); cairo_move_to(cr, cx + 10, cy); cairo_line_to(cr, cx + 7, cy + 3);
	cairo_move_to(cr, cx, cy - 10); cairo_line_to(cr, cx, cy + 10);
	cairo_move_to(cr, cx - 10, cy); cairo_line_to(cr, cx + 10, cy);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, 2.4);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_set_line_width(cr, 1.3);
	cairo_stroke(cr);
}

/* --- grabbing (versão "fechada": círculo cheio central) --------------- */
static void draw_grabbing(cairo_t *cr) {
	double cx = 12, cy = 12;
	cairo_arc(cr, cx, cy, 6.5, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, 1.6);
	cairo_stroke(cr);
	/* 4 tracinhos curtos ao redor, sugerindo "segurando" */
	double arms[4][2] = {{0,-9},{0,9},{-9,0},{9,0}};
	for (int i = 0; i < 4; i++) {
		double dx = arms[i][0], dy = arms[i][1];
		double len = sqrt(dx*dx+dy*dy);
		double ux = dx/len, uy = dy/len;
		cairo_move_to(cr, cx+ux*6.8, cy+uy*6.8);
		cairo_line_to(cr, cx+ux*9.2, cy+uy*9.2);
	}
	cairo_set_line_width(cr, 1.8);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_stroke(cr);
}

/* --- wait (spinner estático: anel 3/4) -------------------------------- */
static void draw_wait(cairo_t *cr) {
	double cx = 12, cy = 12, r = 8;
	cairo_set_line_width(cr, 2.6);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgba(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B, 0.35);
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_arc(cr, cx, cy, r, -M_PI/2, M_PI);
	cairo_stroke(cr);
}

/* --- not-allowed (círculo cortado) ------------------------------------ */
static void draw_not_allowed(cairo_t *cr) {
	double cx = 12, cy = 12, r = 8.5;
	cairo_set_line_width(cr, 2.2);
	cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_DANGER_R, COL_DANGER_G, COL_DANGER_B);
	cairo_set_line_width(cr, 1.1);
	cairo_stroke(cr);
	double d = r * 0.70710678;
	cairo_move_to(cr, cx - d, cy - d);
	cairo_line_to(cr, cx + d, cy + d);
	cairo_set_source_rgb(cr, COL_DANGER_R, COL_DANGER_G, COL_DANGER_B);
	cairo_set_line_width(cr, 2.0);
	cairo_stroke(cr);
}

/* --- crosshair --------------------------------------------------------- */
static void draw_crosshair(cairo_t *cr) {
	double cx = 12, cy = 12;
	cairo_move_to(cr, cx - 9, cy); cairo_line_to(cr, cx + 9, cy);
	cairo_move_to(cr, cx, cy - 9); cairo_line_to(cr, cx, cy + 9);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, 1.6);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_set_line_width(cr, 0.8);
	cairo_stroke(cr);
	cairo_arc(cr, cx, cy, 2.2, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_stroke(cr);
}

static void draw_double_arrow(cairo_t *cr, double angle_deg) {
	double cx = 12, cy = 12;
	cairo_save(cr);
	cairo_translate(cr, cx, cy);
	cairo_rotate(cr, angle_deg * M_PI / 180.0);
	/* haste */
	cairo_move_to(cr, 0, -9); cairo_line_to(cr, 0, 9);
	/* ponta de cima */
	cairo_move_to(cr, 0, -9); cairo_line_to(cr, -3.2, -5.5);
	cairo_move_to(cr, 0, -9); cairo_line_to(cr, 3.2, -5.5);
	/* ponta de baixo */
	cairo_move_to(cr, 0, 9); cairo_line_to(cr, -3.2, 5.5);
	cairo_move_to(cr, 0, 9); cairo_line_to(cr, 3.2, 5.5);
	cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
	cairo_set_line_width(cr, 2.2);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, COL_LINE_R, COL_LINE_G, COL_LINE_B);
	cairo_set_line_width(cr, 1.1);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void draw_ns_resize(cairo_t *cr)   { draw_double_arrow(cr, 0);  }
static void draw_ew_resize(cairo_t *cr)   { draw_double_arrow(cr, 90); }
static void draw_nesw_resize(cairo_t *cr) { draw_double_arrow(cr, -45); }
static void draw_nwse_resize(cairo_t *cr) { draw_double_arrow(cr, 45); }

static const cursor_def_t CURSORS[] = {
	{"default",      draw_default,      2.0,  2.0},
	{"text",         draw_text,         12.0, 12.0},
	{"pointer",      draw_pointer,      9.0,  2.0},
	{"grab",         draw_grab,         12.0, 12.0},
	{"grabbing",     draw_grabbing,     12.0, 12.0},
	{"wait",         draw_wait,         12.0, 12.0},
	{"not-allowed",  draw_not_allowed,  12.0, 12.0},
	{"crosshair",    draw_crosshair,    12.0, 12.0},
	{"ns-resize",    draw_ns_resize,    12.0, 12.0},
	{"ew-resize",    draw_ew_resize,    12.0, 12.0},
	{"nesw-resize",  draw_nesw_resize,  12.0, 12.0},
	{"nwse-resize",  draw_nwse_resize,  12.0, 12.0},
};
#define N_CURSORS (sizeof(CURSORS) / sizeof(CURSORS[0]))

static const int SIZES[] = {24, 48};
#define N_SIZES (sizeof(SIZES) / sizeof(SIZES[0]))

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "uso: %s <diretorio de saida>\n", argv[0]);
		return 1;
	}
	const char *outdir = argv[1];

	for (size_t i = 0; i < N_CURSORS; i++) {
		for (size_t s = 0; s < N_SIZES; s++) {
			int size = SIZES[s];
			double scale = size / 24.0;

			cairo_surface_t *surf = cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32, size, size);
			cairo_t *cr = cairo_create(surf);
			cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
			cairo_paint(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			cairo_scale(cr, scale, scale);

			CURSORS[i].draw(cr);

			char path[512];
			snprintf(path, sizeof(path), "%s/%s-%d.png",
				outdir, CURSORS[i].name, size);
			cairo_status_t st = cairo_surface_write_to_png(surf, path);
			if (st != CAIRO_STATUS_SUCCESS) {
				fprintf(stderr, "erro escrevendo %s: %s\n",
					path, cairo_status_to_string(st));
				return 1;
			}
			cairo_destroy(cr);
			cairo_surface_destroy(surf);
		}
	}
	printf("Gerados %zu formatos x %zu tamanhos = %zu PNGs em %s\n",
		N_CURSORS, N_SIZES, N_CURSORS * N_SIZES, outdir);
	return 0;
}
