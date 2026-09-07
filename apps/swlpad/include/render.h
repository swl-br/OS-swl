#ifndef SWLPAD_RENDER_H
#define SWLPAD_RENDER_H

#include <cairo/cairo.h>
#include "buffer.h"

/*
 * render: desenha o buffer de texto do SWLPad numa superfície cairo.
 * Paleta do theme.h do swl-ui (mesma identidade visual do sistema).
 * Barra de status embaixo com nome do arquivo e posição do cursor.
 */

typedef struct swlpad_render swlpad_render;

swlpad_render *swlpad_render_new(int width, int height);
void swlpad_render_free(swlpad_render *r);

/* desenha o buffer + cursor + barra de status. cursor_on controla blink. */
void swlpad_render_draw(swlpad_render *r, swlpad_buffer *buf,
        const char *filename, bool cursor_on);

cairo_surface_t *swlpad_render_surface(swlpad_render *r);
int swlpad_render_cell_w(swlpad_render *r);
int swlpad_render_cell_h(swlpad_render *r);

/* área de texto exclui a barra de status */
#define SWLPAD_STATUS_H 22
#define SWLPAD_PAD      6

#endif /* SWLPAD_RENDER_H */
