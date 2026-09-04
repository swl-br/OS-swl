#ifndef TSWL_RENDER_H
#define TSWL_RENDER_H

#include <cairo/cairo.h>
#include "term.h"

/*
 * render: desenha o grid do terminal numa superfície cairo (que o main
 * copia pro buffer shm do Wayland). Paleta puxada do theme.h do swl-ui
 * pra identidade visual consistente com o resto do sistema.
 */

typedef struct tswl_render tswl_render;

/* width/height em pixels da área de texto; as métricas da fonte (cw/ch)
 * são medidas uma vez na criação e atualizadas em resize. */
tswl_render *tswl_render_new(int width, int height);
void tswl_render_free(tswl_render *r);

/* desenha o estado atual do term na superfície interna. cursor_on
 * controla o blink (main alterna num timer de ~500ms). */
void tswl_render_draw(tswl_render *r, tswl_term *t, bool cursor_on);

/* superfície cairo pronta pra copiar pro buffer Wayland */
cairo_surface_t *tswl_render_surface(tswl_render *r);

/* métricas da célula em pixels */
int tswl_render_cell_w(tswl_render *r);
int tswl_render_cell_h(tswl_render *r);

/* quantas colunas/linhas cabem numa área w x h pixels */
int tswl_render_cols_for(tswl_render *r, int width);
int tswl_render_rows_for(tswl_render *r, int height);

/* padding interno (pixels) entre a borda da janela e a área de texto */
#define TSWL_RENDER_PAD 4

#endif /* TSWL_RENDER_H */
