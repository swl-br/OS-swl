#ifndef SWL_APPKIT_CONTROLS_H
#define SWL_APPKIT_CONTROLS_H

#include <cairo/cairo.h>
#include <stdbool.h>
#include "swl_theme.h"

/* ===== Botão (pill) ===== */
typedef enum { SWL_BTN_PRIMARY, SWL_BTN_SECONDARY, SWL_BTN_DESTRUCTIVE } swl_btn_kind_t;
void swl_button_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      double w, double h, const char *label, swl_btn_kind_t kind);

/* ===== Campo de texto ===== */
void swl_textfield_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, double h, const char *text,
                         const char *placeholder, bool focused, bool password);

/* ===== Checkbox ===== */
#define SWL_CHECKBOX_SIZE 16
void swl_checkbox_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, bool checked);

/* ===== Radio ===== */
#define SWL_RADIO_SIZE 14
void swl_radio_draw(cairo_t *cr, const swl_theme_t *th, double x, double y, bool selected);

/* ===== Toggle (pill iOS-like) -- animável ===== */
#define SWL_TOGGLE_W 44
#define SWL_TOGGLE_H 24
typedef struct { bool on; double anim; } swl_toggle_t;
void swl_toggle_init(swl_toggle_t *t, bool initial_on);
bool swl_toggle_set(swl_toggle_t *t, bool on);
void swl_toggle_tick(swl_toggle_t *t, int dt_ms);
bool swl_toggle_animating(const swl_toggle_t *t);
void swl_toggle_draw(cairo_t *cr, const swl_theme_t *th, const swl_toggle_t *t, double x, double y);

/* ===== Segmentado (pill contínuo, 2-4 opções, "balão") ===== */
void swl_segmented_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                         double w, double h, const char *const *options, int n, int selected);
/* Índice da opção sob (px,py), ou -1 se fora. */
int swl_segmented_hit(double x, double y, double w, double h, int n, double px, double py);

/* ===== Slider ===== */
void swl_slider_draw(cairo_t *cr, const swl_theme_t *th, double x, double y,
                      double w, double h, double value01);

#endif
