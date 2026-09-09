#ifndef SWL_MENUBAR_H
#define SWL_MENUBAR_H

#include <stdbool.h>
#include <cairo/cairo.h>

/*
 * menubar: barra de menu clássica por aplicativo (Arquivo, Editar,
 * Ver, Configurar…) — M1. Componente reutilizável dos apps nativos,
 * independente de Wayland: só desenha (cairo/pango) e resolve
 * hit-test/estado. O app integra chamando pointer/key e executa as
 * ações retornadas por id.
 *
 * A paleta espelha a do swl-ui/theme.h (fundo #0e1219, texto #d4dee6,
 * ciano #6bd1cc) pra manter a identidade visual do sistema; os
 * valores são copiados de propósito (a biblioteca não pode depender do
 * swl-ui, que é o compositor).
 *
 * Modelo de dados: o app declara menus/itens estáticos (label, id,
 * enabled) e passa pro menubar. O menubar é dono só do estado de
 * interação (hover, aberto, dropdown calculado)
 * geometria da barra em si.
 */

/* Item de um menu. `id` é o código da ação retornado ao app quando o
 * item é ativado (o app escolhe; 0 = sem ação). Se `label` for NULL
 * vira separador horizontal. `enabled` = false desenha o item
 * esmaecido e bloqueia a ativação (placeholder honesto de "em breve"). */
typedef struct swl_menuitem {
    const char *label;
    int id;
    bool enabled;
} swl_menuitem;

typedef struct swl_menu {
    const char *title;
    const swl_menuitem *items;
    int item_count;
} swl_menu;

typedef struct swl_menubar swl_menubar;

/* altura da barra em pixels (constante do componente). */
#define SWL_MENUBAR_BAR_H 26

/* Cria o menubar com `menu_count` menus. `width` é a largura da
 * superfície onde ele será desenhado (recalcular em resize via
 * swl_menubar_resize. As structs menus/itens podem ser estáticas do
 * app — o menubar guarda só os ponteiros, não copia. */
swl_menubar *swl_menubar_new(int width, const swl_menu *menus, int menu_count);
void swl_menubar_free(swl_menubar *mb);

void swl_menubar_resize(swl_menubar *mb, int width);

/* Desenha a barra (e o dropdown aberto, se houver) num contexto
 * cairo cujo (0,0) é o topo da janela do app. Hey o dropdown é
 * desenhado por baixo da barra mesmos;w é a largura da superfície. */
void swl_menubar_draw(swl_menubar *mb, cairo_t *cr, int surface_w);

/* Ponteiro: chame com (sx,sy) em coordenadas da janela do app. */
void swl_menubar_pointer_motion(swl_menubar *mb, int x, int y);

/* Clique. Retorna o id da ação se o clique ativou um item (>0),
 * ou 0 se não ativou nada (ex.: abriu/fechou menu, clicou fora, ou
 * clicou num item desabilitado. `pressed` distingue pressão de
 * soltura — ações executam soltura (padrão clássico de menu). */
int swl_menubar_pointer_button(swl_menubar *mb, int x, int y, bool pressed);

bool swl_menubar_is_open(swl_menubar *mb);

/* Teclado (navegação clássica: Alt abre o primeiro menu, setas
 * navegam entre menus/itens, Enter ativa, Esc fecha). O app traduz
 * keysyms xkb pra estes códigos (evita dependência de xkbcommon na
 * biblioteca).. Retorna id de ação, ou 0. */
enum swl_menubar_key {
    SWL_MENUBAR_KEY_ALT,
    SWL_MENUBAR_KEY_LEFT,
    SWL_MENUBAR_KEY_RIGHT,
    SWL_MENUBAR_KEY_UP,
    SWL_MENUBAR_KEY_DOWN,
    SWL_MENUBAR_KEY_ENTER,
    SWL_MENUBAR_KEY_ESC,
};
int swl_menubar_key(swl_menubar *mb, enum swl_menubar_key key);

#endif /* SWL_MENUBAR_H */