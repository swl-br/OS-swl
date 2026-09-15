#ifndef SWLSYSINFO_RENDER_H
#define SWLSYSINFO_RENDER_H

#include <cairo/cairo.h>
#include "monitor.h"

/*
 * render: desenha o painel de monitoramento numa surface cairo que o main
 * copia pro buffer shm do Wayland. Paleta puxada do theme.h do swl-ui
 * (fundo #0b0e14, texto #d4dee6, ciano #6bd1cc, etc.) pra
 * identidade visual consistente com o resto do sistema.

 */

typedef struct swlsysinfo_render swlsysinfo_render;

swlsysinfo_render *swlsysinfo_render_new(int width, int height);
void swlsysinfo_render_free(swlsysinfo_render *r);

/* Desenha o snapshot atual na surface interna. surface pronta pra copiar. */
void swlsysinfo_render_draw(swlsysinfo_render *r, const struct swlsysinfo_snapshot *snap);
cairo_surface_t *swlsysinfo_render_surface(swlsysinfo_render *r);

#endif /* SWLSYSINFO_RENDER_H */