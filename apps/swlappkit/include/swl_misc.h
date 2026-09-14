#ifndef SWL_APPKIT_MISC_H
#define SWL_APPKIT_MISC_H

#include <cairo/cairo.h>
#include <stdbool.h>
#include "swl_theme.h"

/* ===== Abas ===== */
double swl_tabs_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      const char *const *labels, int n, int selected);
int swl_tabs_hit(double x, double y, double h, const char *const *labels, int n, double px, double py);

/* ===== Tag / badge ===== */
void swl_tag_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                   const char *label, swl_color_t accent);

/* ===== Barra de progresso ===== */
void swl_progress_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                        double w, double h, double value01);

/* ===== Tooltip ===== */
void swl_tooltip_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, const char *text);

/* ===== Banner / alerta persistente (diferente do toast: fica fixo,
 * não desaparece sozinho) ===== */
typedef enum { SWL_BANNER_INFO, SWL_BANNER_WARN, SWL_BANNER_ERROR } swl_banner_kind_t;
void swl_banner_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, double w,
                      const char *text, swl_banner_kind_t kind);
#define SWL_BANNER_H 40

/* ===== Menu de contexto ===== */
typedef struct { const char *label; bool destructive; bool enabled; } swl_ctxitem_t;
double swl_context_menu_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                              const swl_ctxitem_t *items, int n, int hover);
int swl_context_menu_hit(double x, double y, int n, double px, double py);

/* ===== Seção recolhível (accordion) ===== */
void swl_accordion_header_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                                double w, double h, const char *label, bool open);

/* ===== Chip de atalho de teclado ===== */
double swl_shortcut_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                          const char *const *keys, int n);

/* ===== Status com ponto colorido ===== */
void swl_status_dot_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                          const char *label, swl_color_t color);

/* ===== Contador numérico (+/-) ===== */
void swl_counter_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                       double w, double h, int value);

/* ===== Lista / tabela (cabeçalho + linhas simples) ===== */
void swl_table_header_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                            double w, const char *const *cols, const double *col_w, int n);
void swl_table_row_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, const char *const *cells, const double *col_w, int n,
                         const swl_color_t *cell_colors /* pode ser NULL */, bool zebra);
#define SWL_TABLE_ROW_H 32

#endif
