#ifndef SWL_APPKIT_THEME_H
#define SWL_APPKIT_THEME_H

#include <cairo/cairo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * swl_theme.h -- paleta e constantes canônicas de TODOS os apps do
 * SWL OS. Qualquer app novo inclui só este header pra ter acesso ao
 * padrão visual completo (cores, raio de cantos, fonte).
 *
 * O raio SWL_CONTENT_RADIUS_BOTTOM tem que bater exatamente com
 * DECO_RADIUS de swl-ui/src/decorations.c -- é assim que a quina de
 * baixo da janela (desenhada pelo app) combina com a quina de cima
 * (desenhada pelo compositor) sem ficar torto. Se um dia mudar o raio
 * da decoração, muda aqui também.
 */

typedef struct { double r, g, b, a; } swl_color_t;

#define SWL_RADIUS_WINDOW   10.0 /* deve bater com DECO_RADIUS em decorations.c */
#define SWL_RADIUS_PANEL    12.0
#define SWL_RADIUS_CONTROL  999.0 /* "pill" -- metade da altura, sempre arredonda total */
#define SWL_RADIUS_SMALL    8.0

/* ---- tema escuro (padrão do sistema) ---- */
#define SWLD_BG            ((swl_color_t){0.043, 0.055, 0.078, 1.00}) /* #0b0e14 */
#define SWLD_PANEL         ((swl_color_t){0.071, 0.082, 0.106, 1.00}) /* #12151c */
#define SWLD_PANEL_2       ((swl_color_t){0.090, 0.106, 0.133, 1.00}) /* #171b22 */
#define SWLD_BORDER        ((swl_color_t){1.0, 1.0, 1.0, 0.08})
#define SWLD_TEXT          ((swl_color_t){0.843, 0.863, 0.886, 1.00}) /* #d7dce2 */
#define SWLD_TEXT_DIM      ((swl_color_t){0.486, 0.529, 0.580, 1.00}) /* #7c8794 */
#define SWLD_TEXT_FAINT    ((swl_color_t){0.290, 0.322, 0.369, 1.00}) /* #4a525e */

/* ---- tema claro ---- */
#define SWLL_BG            ((swl_color_t){0.914, 0.922, 0.937, 1.00}) /* #e9ebef */
#define SWLL_PANEL         ((swl_color_t){0.965, 0.969, 0.976, 1.00}) /* #f6f7f9 */
#define SWLL_PANEL_2       ((swl_color_t){0.925, 0.933, 0.945, 1.00})
#define SWLL_BORDER        ((swl_color_t){0.0, 0.0, 0.0, 0.10})
#define SWLL_TEXT          ((swl_color_t){0.11, 0.13, 0.16, 1.00})
#define SWLL_TEXT_DIM      ((swl_color_t){0.42, 0.46, 0.51, 1.00})
#define SWLL_TEXT_FAINT    ((swl_color_t){0.58, 0.61, 0.65, 1.00})

/* ---- acentos (iguais nos dois temas) ---- */
#define SWL_CYAN    ((swl_color_t){0.420, 0.820, 0.800, 1.00}) /* #6bd1cc */
#define SWL_RED     ((swl_color_t){0.878, 0.341, 0.310, 1.00}) /* #e0574f */
#define SWL_AMBER   ((swl_color_t){0.878, 0.635, 0.247, 1.00}) /* #e0a23f */
#define SWL_PURPLE  ((swl_color_t){0.545, 0.420, 0.820, 1.00}) /* #8b6bd1 */

#define SWL_FONT "JetBrains Mono, Fira Code, monospace"

/* Estado do tema em runtime -- todo app chama swl_theme_load() uma vez
 * no início (ou quando um sinal de troca de tema chegar) pra ler
 * ~/.config/swl/theme, o mesmo arquivo que swl-ui/decorations.c lê.
 * Isso é o mecanismo PROVISÓRIO até existir o swl-bus; centralizado
 * aqui pra só precisar trocar num lugar quando o swl-bus existir. */
typedef struct {
    bool light;
    swl_color_t bg, panel, panel2, border, text, text_dim, text_faint;
} swl_theme_t;

static inline swl_theme_t swl_theme_load(void) {
    swl_theme_t t;
    t.light = false;
    const char *home = getenv("HOME");
    if (home && *home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.config/swl/theme", home);
        FILE *f = fopen(path, "r");
        if (f) {
            char line[16] = {0};
            if (fgets(line, sizeof(line), f)) {
                t.light = strncmp(line, "light", 5) == 0;
            }
            fclose(f);
        }
    }
    if (t.light) {
        t.bg = SWLL_BG; t.panel = SWLL_PANEL; t.panel2 = SWLL_PANEL_2;
        t.border = SWLL_BORDER; t.text = SWLL_TEXT; t.text_dim = SWLL_TEXT_DIM;
        t.text_faint = SWLL_TEXT_FAINT;
    } else {
        t.bg = SWLD_BG; t.panel = SWLD_PANEL; t.panel2 = SWLD_PANEL_2;
        t.border = SWLD_BORDER; t.text = SWLD_TEXT; t.text_dim = SWLD_TEXT_DIM;
        t.text_faint = SWLD_TEXT_FAINT;
    }
    return t;
}

static inline void swl_set(cairo_t *cr, swl_color_t c) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}
#define SWL_SET(cr, C) swl_set((cr), (C))

#endif /* SWL_APPKIT_THEME_H */
