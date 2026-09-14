#include <stdio.h>
#include <string.h>
#include "central.h"

extern void cc_page_draw(cc_app_t *a, cairo_t *cr, int rx, int ry, int rw, int *out_content_h);

int main(int argc, char **argv) {
    cc_app_t a = {0};
    a.width = 760; a.height = 620;
    a.advanced = true;
    cc_api_conf_load(&a);
    a.theme = swl_theme_load();
    static const swl_menuitem m1[] = { {"Backup agora",1,true} };
    static const swl_menu menus[] = { {"Arquivo", m1, 1} };
    a.menubar = swl_menubar_new(a.width, menus, 1);

    int sel = argc > 1 ? atoi(argv[1]) : 0;
    a.sel = (cc_cat_t)sel;
    const char *out = argc > 2 ? argv[2] : "/tmp/shot.png";

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, a.width, a.height);
    a.cr = cairo_create(surf);
    cc_draw_all(&a);
    cairo_surface_write_to_png(surf, out);
    printf("OK %s\n", out);
    return 0;
}
