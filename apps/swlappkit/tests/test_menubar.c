/* test_menubar.c - teste headless do componente swl_menubar (M1). */
/* Sem Wayland: hit-test, abrir/fechar, acoes de clique e teclado. */
/* Tambem desenha um PNG pra inspecao visual fora de display. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <cairo/cairo.h>
#include "menubar.h"

static const swl_menuitem arquivo[] = {
    { "Novo", 1, true },
    { "Salvar", 2, true },
    { "Sair",  3, true },
};
static const swl_menuitem editar[] = {
    { "Copiar",  4, false },
    { "Colar",   5, false },
};
static const swl_menuitem config[] = {
    { "Preferencias...", 6, false },
};
static const swl_menu menus[] = {
    { "Arquivo",   arquivo,  3 },
    { "Editar",    editar,   2 },
    { "Configurar", config,   1 },
};

static int item_y(int row) {
    return SWL_MENUBAR_BAR_H + 6 + row * 24;
}

static void test_abre_aciona(void) {
    swl_menubar *mb = swl_menubar_new(640, menus,  3);
    assert(mb);
    assert(!swl_menubar_is_open(mb));
    int r = swl_menubar_pointer_button(mb, 20, 10, true);
    assert(r == 0);
    assert(swl_menubar_is_open(mb));
    r = swl_menubar_pointer_button(mb, 20,  item_y(2), false);
    assert(r == 3);  /* Sair */
    assert(!swl_menubar_is_open(mb));
    swl_menubar_free(mb);
}

static void test_fora_fecha(void) {
    swl_menubar *mb = swl_menubar_new(640, menus,  3);
    int r = swl_menubar_pointer_button(mb, 20, 10, true);
    assert(r == 0);
    assert(swl_menubar_is_open(mb));
    r = swl_menubar_pointer_button(mb, 600, 300, true);
    assert(r == 0);
    assert(!swl_menubar_is_open(mb));
    swl_menubar_free(mb);
}

static void test_desabilitado(void) {
    swl_menubar *mb = swl_menubar_new(640, menus,  3);
    int r = swl_menubar_key(mb, SWL_MENUBAR_KEY_ALT);
    r = swl_menubar_key(mb, SWL_MENUBAR_KEY_RIGHT);
    r = swl_menubar_key(mb, SWL_MENUBAR_KEY_DOWN);
    assert(swl_menubar_is_open(mb));
    r = swl_menubar_key(mb, SWL_MENUBAR_KEY_ENTER);
    assert(r == 0);
    assert(swl_menubar_is_open(mb));
    r = swl_menubar_pointer_button(mb, 600, 300, true);
    assert(r == 0);
    assert(!swl_menubar_is_open(mb));
    swl_menubar_free(mb);
}

static void test_teclado(void) {
    swl_menubar *mb = swl_menubar_new(640, menus,  3);
    int r = swl_menubar_key(mb, SWL_MENUBAR_KEY_ALT);
    assert(r == 0);
    assert(swl_menubar_is_open(mb));
    r = swl_menubar_key(mb, SWL_MENUBAR_KEY_DOWN);
    r = swl_menubar_key(mb, SWL_MENUBAR_KEY_ENTER);
    assert(r == 1);
    assert(!swl_menubar_is_open(mb));
    swl_menubar_free(mb);
}

static void test_desenha_png(void) {
    swl_menubar *mb = swl_menubar_new(640, menus,  3);
    swl_menubar_pointer_button(mb, 20, 10, true);
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 640,  400);
    cairo_t *cr = cairo_create(surf);
    cairo_set_source_rgb(cr, 0.043, 0.055, 0.078);
    cairo_paint(cr);
    swl_menubar_draw(mb, cr,  640);
    cairo_surface_write_to_png(surf,  "menubar_test.png");
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    swl_menubar_free(mb);
}

int main(void) {
    test_abre_aciona();
    test_fora_fecha();
    test_desabilitado();
    test_teclado();
    test_desenha_png();
    printf("menubar: testes ok\n");
    return 0;
}
