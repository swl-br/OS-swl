#ifndef SWL_APPKIT_OVERLAYS_H
#define SWL_APPKIT_OVERLAYS_H

#include <cairo/cairo.h>
#include <stdbool.h>
#include "swl_theme.h"
#include "swl_controls.h"

/* ===== Modal / diálogo ===== */
/* Desenha o véu escuro sobre (0,0,win_w,win_h) + o painel centralizado
 * com título, corpo e até 2 botões. Preenche os retângulos de saída
 * dos botões (em coordenadas da janela) pro app fazer hit-test com
 * swl_button_hit(). cancel_label pode ser NULL (só 1 botão). */
void swl_modal_draw(cairo_t *cr, const swl_theme_t *th, int win_w, int win_h,
                     const char *title, const char *body,
                     const char *ok_label, swl_btn_kind_t ok_kind,
                     const char *cancel_label,
                     double *ok_x, double *ok_y, double *ok_w, double *ok_h,
                     double *cancel_x, double *cancel_y, double *cancel_w, double *cancel_h,
                     double *panel_x, double *panel_y, double *panel_w, double *panel_h);

bool swl_rect_hit(double x, double y, double w, double h, double px, double py);

/* ===== Toast (notificação) ===== */
typedef enum { SWL_TOAST_INFO, SWL_TOAST_SUCCESS, SWL_TOAST_WARN, SWL_TOAST_ERROR } swl_toast_kind_t;
/* Desenha em (x,y) com largura w; altura é sempre 46. `sub` (linha
 * secundária, ex. "agora mesmo") pode ser NULL. */
void swl_toast_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, double w,
                     const char *title, const char *sub, swl_toast_kind_t kind);
#define SWL_TOAST_H 46

/* ===== Dropdown / select ===== */
/* Botão fechado (pill com o valor atual + chevron). */
void swl_dropdown_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                        double w, double h, const char *value, bool open);
/* Lista de opções aberta, logo abaixo do botão (dx,dy = canto do
 * botão; a lista nasce em dy+h). Retorna a altura desenhada. */
double swl_dropdown_list_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                               double w, const char *const *options, int n, int hover);
int swl_dropdown_list_hit(double x, double y, double w, int n, double px, double py);

#endif
