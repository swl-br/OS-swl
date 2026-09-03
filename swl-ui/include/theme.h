#ifndef SWL_THEME_H
#define SWL_THEME_H

#include <cairo/cairo.h>

/*
 * Paleta "hacker retrô" do SWL OS.
 *
 * Cada cor é um compound literal swl_color_t{r,g,b,a} (0..1), pensado para
 * ser usado com a macro SWL_SET(cr, NOME_DA_COR) abaixo.
 *
 * Nota histórica: a versão anterior definia cada cor como uma lista solta
 * de 4 doubles separados por vírgula (ex.: "0.42, 0.82, 0.80, 1.00") e
 * SWL_SET só colava isso nos parênteses de swl_set_color(). Isso quebra
 * silenciosamente sempre que a cor aparece dentro de um ternário, como em
 *   SWL_SET(cr, focused ? SWL_COL_ACCENT_CYAN : SWL_COL_TEXT_DIM);
 * porque a gramática do C permite o operador vírgula dentro do ramo do
 * meio do ternário: o resultado expandido vira
 *   swl_set_color(cr, focused ? 0.42,0.82,0.80,1.00 : 0.45, 0.50,0.56,1.00)
 * que o compilador lê como "swl_set_color(cr, (focused?1.00:0.45), 0.50, 0.56, 1.00)"
 * — os componentes r/g/b da cor de foco somem, e a cor "não focado" some
 * quase inteira, sobrando só um warning de "comma expression" pra avisar.
 * Agrupando cada cor num struct via compound literal, o ternário compara
 * dois valores do mesmo tipo normalmente e o bug desaparece.
 */
typedef struct { double r, g, b, a; } swl_color_t;

#define SWL_COL_BG            ((swl_color_t){0.043, 0.055, 0.078, 1.00}) /* fundo geral #0b0e14    */
#define SWL_COL_PANEL_BG      ((swl_color_t){0.055, 0.070, 0.098, 0.96}) /* barras/painéis #0e1219 */
#define SWL_COL_PANEL_BORDER  ((swl_color_t){0.20,  0.55,  0.58,  0.55}) /* borda ciano sutil      */
#define SWL_COL_ACCENT_CYAN   ((swl_color_t){0.42,  0.82,  0.80,  1.00}) /* #6bd1cc                */
#define SWL_COL_ACCENT_PURPLE ((swl_color_t){0.75,  0.62,  0.86,  1.00}) /* #bf9edb                */
#define SWL_COL_ACCENT_GOLD   ((swl_color_t){0.83,  0.65,  0.36,  1.00}) /* #d4a55c (pastas)       */
#define SWL_COL_TEXT          ((swl_color_t){0.83,  0.87,  0.90,  1.00}) /* #d4dee6                */
#define SWL_COL_TEXT_DIM      ((swl_color_t){0.45,  0.50,  0.56,  1.00}) /* #737f8f                */
#define SWL_COL_DANGER        ((swl_color_t){0.86,  0.35,  0.40,  1.00}) /* botão fechar           */

/* Monta uma cor ad-hoc fora da paleta fixa, ex.: SWL_SET(cr, SWL_COLOR(0.15, 0.35, 0.38, 0.12)); */
#define SWL_COLOR(R, G, B, A) ((swl_color_t){(R), (G), (B), (A)})

/* Fonte usada em toda a interface do sistema. Ajuste para a que estiver
 * instalada no seu sistema; "monospace" é o fallback seguro do fontconfig. */
#define SWL_FONT_MONO "JetBrains Mono, Fira Code, monospace"

/* Métricas fixas dos elementos de shell (em pixels lógicos). */
#define SWL_PANEL_HEIGHT     26
#define SWL_TASKBAR_HEIGHT   30
#define SWL_TITLEBAR_HEIGHT  24

static inline void swl_set_color(cairo_t *cr, swl_color_t c) {
	cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}

/* Uso: SWL_SET(cr, SWL_COL_ACCENT_CYAN);
 *      SWL_SET(cr, focused ? SWL_COL_ACCENT_CYAN : SWL_COL_TEXT_DIM); */
#define SWL_SET(cr, COLOR) swl_set_color((cr), (COLOR))

#endif /* SWL_THEME_H */
