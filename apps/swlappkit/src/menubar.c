/*
 * menubar.c — barra de menu clássica por aplicativo (M1).
 *
 * Implementação: estado puro + desenho cairo/pango. Sem Wayland.
 *
 * Modelo: barra no topo com títulos dos menus, espaçados (Arquivo,
 * Editar, …). Aberto, desenha um dropdown por baixo do título com os
 * itens do menu; hover no título com dropdown aberto troca de menu;
 *
 * clique em item (soltura) retorna o id da ação. Clique fora fecha.
 *
 * A barra inteira (títulos + dropdown aberto) é a área de interação
 * do menubar: enquanto `open`, todo clique na superfície passa por ele
 * primeiro (o app decide se o clique era fora da barra/dropdown e então
 * o menubar fecha e devolve 0 — o app não deve reencaminhar pro
 * conteúdo nesse caso, porque o clique foi na área do topo). Na prática
 * o padrão do app é: se hit dentro da barra OU dropdown aberto → chama
 * pointer_button; se retornar ação executa; senão nada. Se não há
 * dropdown aberto e o clique é fora da barra → nem chama (vai pro
 * conteúdo, ex.: o grid do terminal futuramente).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pango/pangocairo.h>
#include "menubar.h"

#define SWL_MB_FONT "JetBrains Mono, Fira Code, monospace"
#define SWL_MB_FONT_SIZE 11.0
#define SWL_MB_ITEM_H 24
#define SWL_MB_PAD_X 8
#define SWL_MB_DROPDOWN_PAD_V 6
#define SWL_MB_SEP_H 9
#define SWL_MB_MAX_ITEM_W 220

#define SWL_MB_FONT_DESC SWL_MB_FONT " " "11.0"

struct swl_menubar {
    int width;
    int bar_h;
    const swl_menu *menus;
    int menu_count;

    /* estado de interação */
    int hover_menu;      /* -1 = nenhum */
    int open_menu;       /* -1 = nenhum aberto */
    int hover_item;      /* item sob o mouse dentro do dropdown aberto */
    int key_item;        /* item selecionado por teclado (-1 = nenhum) */

    /* geometria calculada (por título) */
    int *title_x;        /* X inicial de cada título */
    int *title_w;
    int total_w;         /* largura total da soma dos títulos */
};

static void free_geoms(struct swl_menubar *mb) {
    free(mb->title_x);
    free(mb->title_w);
    mb->title_x = NULL;
    mb->title_w = NULL;
}

/* mede os títulos e monta a colunas da barra */
static void layout_titles(struct swl_menubar *mb) {


    free_geoms(mb);
    mb->title_x = calloc((size_t)(mb->menu_count > 0 ? mb->menu_count : 1), sizeof(int));
    mb->title_w = calloc((size_t)(mb->menu_count > 0 ? mb->menu_count : 1), sizeof(int));
    if (!mb->title_x || !mb->title_w) {
        free_geoms(mb);
        return;
    }

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);


    int x = SWL_MB_PAD_X;
    mb->total_w = 0;
    for (int i = 0; i < mb->menu_count; i++) {
        pango_layout_set_text(layout, mb->menus[i].title, -1);
        int w, h;
        pango_layout_get_pixel_size(layout, &w, &h);
        if (w < 24) w = 24;
        mb->title_x[i] = x;
        mb->title_w[i] = w + SWL_MB_PAD_X * 2;
        x += mb->title_w[i];
        mb->total_w += mb->title_w[i];
    }
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

swl_menubar *swl_menubar_new(int width, const swl_menu *menus, int menu_count) {



    swl_menubar *mb = calloc(1, sizeof(*mb));
    if (!mb) return NULL;
    mb->width = width < 1 ? 1 : width;
    mb->bar_h = SWL_MENUBAR_BAR_H;
    mb->menus = menus;
    mb->menu_count = menu_count > 0 ? menu_count : 0;
    mb->hover_menu = -1;
    mb->open_menu = -1;
    mb->hover_item = -1;
    mb->key_item = -1;
    if (mb->menu_count > 0) {
        layout_titles(mb);
    }
    return mb;
}

void swl_menubar_free(swl_menubar *mb) {

    if (!mb) return;
    free_geoms(mb);
    free(mb);
}

void swl_menubar_resize(swl_menubar *mb, int width) {

    if (!mb) return;
    mb->width = width < 1 ? 1 : width;
    if (mb->menu_count > 0) {
        layout_titles(mb);
    }
    /* dropdown recalcula sozinho no draw/hit-test (é derivado dos títulos) */
}

bool swl_menubar_is_open(swl_menubar *mb) {

    return mb && mb->open_menu >= 0;
}

static int hit_title(swl_menubar *mb, int x) {



    if (!mb->title_x || x < mb->title_x[0]) return -1;
    for (int i = 0; i < mb->menu_count; i++) {
        if (x >= mb->title_x[i] && x < mb->title_x[i] + mb->title_w[i]) return i;
    }
    return -1;
}

/* retângulo do dropdown do menu `idx` (coordenadas da janela) */
static void dropdown_rect(swl_menubar *mb, int idx, int *x, int *y, int *w, int *h) {



    int item_count = mb->menus[idx].item_count;
    int iw = SWL_MB_MAX_ITEM_W;
    for (int i = 0; i < item_count; i++) {
        if (!mb->menus[idx].items[i].label) continue;
        cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
        cairo_t *cr = cairo_create(surf);
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *fd = pango_font_description_from_string(SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout, mb->menus[idx].items[i].label, -1);
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        if (tw + SWL_MB_PAD_X * 4 > iw) iw = tw + SWL_MB_PAD_X * 4;
    }
    int dy = mb->bar_h;
    int dx = mb->title_x[idx] < SWL_MB_PAD_X ? SWL_MB_PAD_X : mb->title_x[idx];
    int dh = SWL_MB_DROPDOWN_PAD_V * 2 + item_count * SWL_MB_ITEM_H;
    if (dx + iw > mb->width) dx = mb->width - iw;
    if (dx < 0) dx = 0;
    *x = dx;
    *y = dy;
    *w = iw;
    *h = dh;
}

static int hit_dropdown(swl_menubar *mb, int idx, int x, int y) {




    int dx, dy, dw, dh;
    dropdown_rect(mb, idx, &dx, &dy, &dw, &dh);
    if (x >= dx && x < dx + dw && y >= dy && y < dy + dh) {
        int item = (y - dy - SWL_MB_DROPDOWN_PAD_V) / SWL_MB_ITEM_H;
        if (item >= mb->menus[idx].item_count) item = -1;
        return item;

    }
    return -1;

}

static void draw_bar(cairo_t *cr, swl_menubar *mb, int surface_w) {



    /* fundo da barra */
    cairo_set_source_rgba(cr,  0.055,  0.070, 0.098, 0.96);
    cairo_rectangle(cr,0,0,surface_w,mb->bar_h);
    cairo_fill(cr);


    /* linha de baixo da barra */
    cairo_set_source_rgba(cr,0.20,0.55,0.58,0.55);
    cairo_rectangle(cr,0,mb->bar_h-1,surface_w,1);
    cairo_fill(cr);


    if (!mb->menus || mb->menu_count <1) return;


    for (int i =0; i <mb->menu_count; i++){
        int x0 =mb->title_x[i];
        int w =mb->title_w[i];
        bool active = (i ==mb->open_menu) || (i ==mb->hover_menu);
        if (active){
            cairo_set_source_rgba(cr,0.20,0.55,0.58,0.25);
            cairo_rectangle(cr,x0,0,w,mb->bar_h-1);
            cairo_fill(cr);


        }
        cairo_set_source_rgb(cr,active ?0.90 :0.83,active ?0.93 :0.87,active ?0.95 :0.90);
        PangoLayout *layout =pango_cairo_create_layout(cr);
        PangoFontDescription *fd =pango_font_description_from_string(
            SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout,fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout,mb->menus[i].title,-1);
        cairo_move_to(cr,x0 +SWL_MB_PAD_X,(mb->bar_h-10) /2);
        pango_cairo_show_layout(cr,layout);
        g_object_unref(layout);
    }
}

static void draw_dropdown(cairo_t *cr, swl_menubar *mb, int surface_w, int idx){

    int dx, dy, dw, dh;
    dropdown_rect(mb,idx,&dx,&dy,&dw,&dh);
    (void)surface_w;

    /* fundo */
    cairo_set_source_rgba(cr,0.055,0.070,0.098,0.97);
    cairo_rectangle(cr,dx,dy,dw,dh);
    cairo_fill(cr);


    /* borda */
    cairo_set_source_rgba(cr,0.20,0.55,0.58,0.55);
    cairo_set_line_width(cr,1);
    cairo_rectangle(cr,dx+0.5,dy+0.5,dw-1,dh-1);
    cairo_stroke(cr);


    const swl_menuitem *items = mb->menus[idx].items;
    for (int i =0; i <mb->menus[idx].item_count; i++){
        int iy =dy +SWL_MB_DROPDOWN_PAD_V +i *SWL_MB_ITEM_H;
        if (items[i].label ==NULL){/* separador */
            cairo_set_source_rgba(cr,0.20,0.55,0.58,0.35);
            cairo_rectangle(cr,dx +12,iy +SWL_MB_ITEM_H /2,dw-24,1);
            cairo_fill(cr);
            continue;
        }
        bool hovered = (i ==mb->hover_item) || (i ==mb->key_item);
        if (hovered){
            cairo_set_source_rgba(cr,0.20,0.55,0.58,0.22);
            cairo_rectangle(cr,dx +2,iy,dw-4,SWL_MB_ITEM_H);
            cairo_fill(cr);


        }
        cairo_set_source_rgb(cr,items[i].enabled ?0.83 :0.45,items[i].enabled ?0.87 :0.50,items[i].enabled ?0.90 :0.56);
        PangoLayout *layout =pango_cairo_create_layout(cr);
        PangoFontDescription *fd =pango_font_description_from_string(
            SWL_MB_FONT_DESC);
        pango_layout_set_font_description(layout,fd);
        pango_font_description_free(fd);
        pango_layout_set_text(layout,items[i].label,-1);
        cairo_move_to(cr,dx +SWL_MB_PAD_X +6,iy +(SWL_MB_ITEM_H-10)/2);
        pango_cairo_show_layout(cr,layout);
        g_object_unref(layout);
    }
}

void swl_menubar_draw(swl_menubar *mb, cairo_t *cr, int surface_w){

    if (!mb) return;
    draw_bar(cr,mb,surface_w);
    if (mb->open_menu >=0){
        draw_dropdown(cr,mb,surface_w,mb->open_menu);
    }
}

void swl_menubar_pointer_motion(swl_menubar *mb, int x, int y){

    if (!mb || !mb->menus || mb->menu_count <1) return;
    mb->hover_menu = hit_title(mb,x);
    mb->hover_item = -1;
    if (mb->open_menu >=0){
        if (y <mb->bar_h){
            /* sobre a barra: hover nos títulos; se passou pra outro
             * título, troca de menu aberto (padrão clássico). */
            if (mb->hover_menu >=0 && mb->hover_menu !=mb->open_menu){
                mb->open_menu =mb->hover_menu;
                mb->key_item = -1;
            }
        } else{
            mb->hover_item = hit_dropdown(mb,mb->open_menu,x,y);
            mb->hover_menu = mb->open_menu;/* mantém o título ativo */
        }
    }
}

int swl_menubar_pointer_button(swl_menubar *mb, int x, int y, bool pressed){

    if (!mb || !mb->menus || mb->menu_count <1) return 0;
    int mi = hit_title(mb,x);
    int di = mb->open_menu >=0 ? hit_dropdown(mb,mb->open_menu,x,y) : -1;

    if (!pressed){
        /* soltura: executa se estava sobre um item do dropdown aberto */
        if (di >=0){
            const swl_menuitem *item = &mb->menus[mb->open_menu].items[di];
            if (item->label && item->enabled){
                int id =item->id;
                mb->open_menu = -1;
                mb->hover_menu = mi;
                mb->hover_item = -1;
                return id;
            }
        }
        return 0;
    }

    /* pressão */
    if (mb->open_menu >=0){
        if (di >=0){
            /* em cima de um item: mantém aberto até a soltura (retorna
             * na soltura acima). */
            return 0;
        }
        if (mi >=0){
            /* outro título: troca de menu imediatamente */
            mb->open_menu = mi;
            mb->hover_item = -1;
            mb->key_item = -1;
            return 0;
        }
        /* fora do dropdown e da barra: fecha */
        mb->open_menu = -1;
        mb->hover_item = -1;
        mb->key_item = -1;
        return 0;
    }

    /* nada aberto */
    if (mi >=0){
        mb->open_menu = mi;
        mb->hover_item = -1;
        mb->key_item = -1;
        return 0;
    }
    return 0;
}

int swl_menubar_key(swl_menubar *mb, enum swl_menubar_key key){

    if (!mb || !mb->menus || mb->menu_count <1) return 0;

    switch (key){
    case SWL_MENUBAR_KEY_ALT:{
        if (mb->open_menu <0){
            mb->open_menu =mb->hover_menu >=0 ?mb->hover_menu :0;
            mb->key_item = -1;
        } else{
            mb->open_menu = -1;
            mb->key_item = -1;
        }
        return 0;
    }
    case SWL_MENUBAR_KEY_ESC:{
        if (mb->open_menu >=0){
            mb->open_menu = -1;
            mb->key_item = -1;
            return 0;
        }
        return 0;
    }
    case SWL_MENUBAR_KEY_LEFT:
    case SWL_MENUBAR_KEY_RIGHT:{
        if (mb->open_menu <0) return 0;
        int dir = key ==SWL_MENUBAR_KEY_LEFT ? -1 :1;
        int next =mb->open_menu +dir;
        if (next <0 || next >=mb->menu_count){
            next =dir >0 ?0 :mb->menu_count-1;/* wrap */
        }
        mb->open_menu = next;
        mb->key_item = -1;
        mb->hover_item = -1;
        return 0;
    }
    case SWL_MENUBAR_KEY_DOWN:{
        if (mb->open_menu <0) return 0;
        int n =mb->menus[mb->open_menu].item_count;
        if (n <1) return 0;
        int sel =mb->key_item;
        do{
            sel =(sel +1) %n;
        } while (mb->menus[mb->open_menu].items[sel].label ==NULL &&sel !=mb->key_item);
        if (mb->menus[mb->open_menu].items[sel].label ==NULL) return 0;
        mb->key_item = sel;
        mb->hover_item = -1;
        return 0;
    }
    case SWL_MENUBAR_KEY_UP:{
        if (mb->open_menu <0) return 0;
        int n =mb->menus[mb->open_menu].item_count;
        if (n <1) return 0;
        int sel =mb->key_item;
        do{
            sel =(sel -1 +n) %n;
        } while (mb->menus[mb->open_menu].items[sel].label ==NULL &&sel !=mb->key_item);
        if (mb->menus[mb->open_menu].items[sel].label ==NULL) return 0;
        mb->key_item = sel;
        mb->hover_item = -1;
        return 0;
    }
    case SWL_MENUBAR_KEY_ENTER:{
        if (mb->open_menu <0) return 0;
        int sel =mb->key_item;
        if (sel <0){
            /* Enter sem seleção: ativa o primeiro item habilitado */
            for (int i =0; i <mb->menus[mb->open_menu].item_count; i++){
                if (mb->menus[mb->open_menu].items[i].label &&
                    mb->menus[mb->open_menu].items[i].enabled){
                    sel = i;
                    break;
                }
            }
        }
        if (sel <0) return 0;
        const swl_menuitem *item = &mb->menus[mb->open_menu].items[sel];
        if (!item->label || !item->enabled) return 0;
        int id =item->id;
        mb->open_menu = -1;
        mb->key_item = -1;
        return id;
    }
    }
    return 0;
}