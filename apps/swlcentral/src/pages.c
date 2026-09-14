#include <string.h>
#include <stdio.h>
#include "central.h"

typedef enum { HIT_TOGGLE, HIT_BUTTON, HIT_SEGMENTED, HIT_SLIDER_START,
               HIT_DROPDOWN_OPEN, HIT_ROW, HIT_COUNTER_MINUS, HIT_COUNTER_PLUS } hit_kind_t;

typedef struct {
    int x, y, w, h;
    hit_kind_t kind;
    int action;
    int arg;
    bool *bool_dst;
    int *int_dst;      /* segmentado/contador: índice/valor */
    double *dbl_dst;   /* slider */
} hit_t;

static hit_t hits[80];
static int nhits;

typedef struct {
    cc_app_t *a;
    cairo_t *cr;
    int x, w;
    int y;
} cur_t;

static void hit_add(hit_t h) { if (nhits < 80) hits[nhits++] = h; }

#define ROW_H 22
#define GAP 14
#define SECTION_GAP 22

static void section(cur_t *c, const char *label) {
    SWL_SET(c->cr, c->a->theme.text_faint);
    cairo_move_to(c->cr, c->x, c->y);
    cc_draw_small_caption(c->cr, label, c->x, c->y, c->a->theme.text_faint);
    c->y += 26;
    SWL_SET(c->cr, c->a->theme.border);
    cairo_rectangle(c->cr, c->x, c->y - 10, c->w, 1);
    cairo_fill(c->cr);
}

static void row_label(cur_t *c, const char *title, const char *sub) {
    cc_draw_small_caption(c->cr, title, c->x, c->y, c->a->theme.text);
    if (sub) cc_draw_small_caption(c->cr, sub, c->x, c->y + 18, c->a->theme.text_dim);
}

static void row_toggle(cur_t *c, const char *title, const char *sub, bool *dst, int action) {
    row_label(c, title, sub);
    double tx = c->x + c->w - SWL_TOGGLE_W, ty = c->y - 3;
    swl_toggle_t t; swl_toggle_init(&t, *dst);
    swl_toggle_draw(c->cr, &c->a->theme, &t, tx, ty);
    hit_add((hit_t){ (int)tx, (int)ty, SWL_TOGGLE_W, SWL_TOGGLE_H, HIT_TOGGLE, action, 0, dst, NULL, NULL });
    c->y += (sub ? 44 : 30) + GAP;
}

static void row_segmented(cur_t *c, const char *title, const char *const *opts, int n, int *sel, int action) {
    row_label(c, title, NULL);
    c->y += 24;
    double h = 30;
    swl_segmented_draw(c->cr, &c->a->theme, c->x, c->y, c->w, h, opts, n, *sel);
    hit_add((hit_t){ c->x, (int)c->y, c->w, (int)h, HIT_SEGMENTED, action, n, NULL, sel, NULL });
    c->y += h + GAP;
}

static void row_slider(cur_t *c, const char *title, double *val, const char *fmt, int action) {
    char pct[32]; snprintf(pct, sizeof(pct), fmt, (int)(*val * 100));
    cc_draw_small_caption(c->cr, title, c->x, c->y, c->a->theme.text);
    {
        int tw, th;
        cc_text_wh(pct, 12, &tw, &th);
        cc_draw_small_caption(c->cr, pct, c->x + c->w - tw, c->y, c->a->theme.text_dim);
    }
    c->y += 24;
    double h = 18;
    swl_slider_draw(c->cr, &c->a->theme, c->x, c->y, c->w, h, *val);
    hit_add((hit_t){ c->x, (int)c->y, c->w, (int)h, HIT_SLIDER_START, action, 0, NULL, NULL, val });
    c->y += h + GAP + 6;
}

static void row_button(cur_t *c, const char *label, swl_btn_kind_t kind, int action, int arg, double w) {
    double h = 32;
    swl_button_draw(c->cr, &c->a->theme, c->x, c->y, w, h, label, kind);
    hit_add((hit_t){ c->x, (int)c->y, (int)w, (int)h, HIT_BUTTON, action, arg, NULL, NULL, NULL });
    c->y += h + GAP;
}

static void row_dropdown(cur_t *c, const char *title, const char *const *opts, int n, int *sel, int action) {
    row_label(c, title, NULL);
    c->y += 24;
    double h = 32, w = c->w > 220 ? 220 : c->w;
    swl_dropdown_draw(c->cr, &c->a->theme, c->x, c->y, w, h, opts[*sel], false);
    hit_add((hit_t){ c->x, (int)c->y, (int)w, (int)h, HIT_DROPDOWN_OPEN, action, n, NULL, sel, NULL });
    c->y += h + GAP;
}

static void row_status(cur_t *c, const char *label, const char *value, swl_color_t vcolor) {
    row_label(c, label, NULL);
    int tw, th;
    cc_text_wh(value, 12, &tw, &th);
    cc_draw_small_caption(c->cr, value, c->x + c->w - tw, c->y, vcolor);
    c->y += 30 + GAP;
}

static void row_field(cur_t *c, const char *title, char *buf, size_t cap, const char *placeholder) {
    (void)cap;
    row_label(c, title, NULL);
    c->y += 24;
    double h = 32;
    swl_textfield_draw(c->cr, &c->a->theme, c->x, c->y, c->w > 260 ? 260 : c->w, h, buf, placeholder, false, false);
    c->y += h + GAP;
}

/* ================= REDE ================= */
static void draw_rede(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "WI-FI");
    bool wifi_on = a->wifi;
    char ssid[64] = {0}, ip[48] = {0}; int sigp = 0;
    bool connected = cc_api_net_status(ssid, sizeof(ssid), ip, sizeof(ip), &sigp);
    static char sub[96];
    snprintf(sub, sizeof(sub), "Adaptador %s%s%s", wifi_on ? "ligado" : "desligado",
             connected ? " · " : "", connected ? ssid : "");
    row_toggle(c, "Wi-Fi", sub, &a->wifi, CC_ACT_WIFI);
    row_status(c, "Estado", connected ? "Conectada" : "Desconectada",
               connected ? SWL_CYAN : a->theme.text_dim);
    row_status(c, "IPv4", ip[0] ? ip : "Sem endereço", a->theme.text_dim);
    row_button(c, "Buscar redes", SWL_BTN_SECONDARY, CC_ACT_SCAN_WIFI, 0, 180);

    static cc_net_t nets[6];
    int n = cc_api_wifi_scan(nets, 6);
    if (n > 0) {
        section(c, "REDES ENCONTRADAS");
        for (int i = 0; i < n; i++) {
            row_label(c, nets[i].ssid, nets[i].meta);
            if (nets[i].connected) {
                row_status(c, "", "Conectada", SWL_CYAN);
            } else {
                double bw = 100, bh = 28;
                double bx = c->x + c->w - bw, by = c->y - 30;
                swl_button_draw(c->cr, &a->theme, bx, by, bw, bh, "Conectar", SWL_BTN_SECONDARY);
                hit_add((hit_t){ (int)bx, (int)by, (int)bw, (int)bh, HIT_ROW, CC_ACT_NET_CONNECT, i, NULL, NULL, NULL });
                c->y += 44 + GAP - 30;
            }
        }
    }

    section(c, "ETHERNET");
    bool eth_on = false; char iface[64] = {0};
    bool has_eth = cc_api_eth_get(&eth_on, iface, sizeof(iface));
    static char eth_sub[96];
    snprintf(eth_sub, sizeof(eth_sub), has_eth ? "%s · %s" : "Nenhuma interface encontrada",
             iface, eth_on ? "conectado" : "cabo desconectado");
    a->eth = eth_on;
    row_toggle(c, "Ethernet", eth_sub, &a->eth, has_eth ? CC_ACT_ETH : CC_ACT_NONE);

    section(c, "BLUETOOTH");
    row_toggle(c, "Bluetooth", "2 dispositivos pareados", &a->bt, CC_ACT_BT);
}

/* ================= PERSONALIZAÇÃO ================= */
static const char *THEME_OPTS[3] = { "Claro", "Escuro", "Auto" };
static void draw_person(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "TEMA");
    row_segmented(c, "Modo", THEME_OPTS, 3, &a->theme_mode, CC_ACT_THEME);
    row_status(c, "Destaque", "Ciano · #6bd1cc", SWL_CYAN);

    section(c, "IDIOMA");
    row_dropdown(c, "Idioma do sistema", cc_lang_opts, 3, &a->lang, CC_ACT_LANG);

    section(c, "INTERFACE");
    row_slider(c, "Transparência", &a->transp, "%d%%", CC_ACT_TRANSP);
    row_slider(c, "Escala", &a->scale, "%d%%", CC_ACT_SCALE);
    row_toggle(c, "Reduzir animações", "Desativa efeitos de transição", &a->reduce_motion, CC_ACT_REDUCE_MOTION);
}

/* ================= SISTEMA ================= */
static const char *POWER_OPTS[3] = { "Economia", "Balanceado", "Desempenho" };
static void draw_sistema(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "INFORMAÇÕES");
    row_status(c, "Sistema", "SWL OS 0.6.0", a->theme.text_dim);
    row_status(c, "Kernel", "Linux 7.2.1 i386", a->theme.text_dim);
    row_field(c, "Hostname", a->hostname, sizeof(a->hostname), "swl-os");

    section(c, "ENERGIA");
    row_segmented(c, "Perfil", POWER_OPTS, 3, &a->power_profile, CC_ACT_POWER);
    row_slider(c, "Brilho da tela", &a->bright, "%d%%", CC_ACT_BRIGHT);
    row_toggle(c, "Suspender na tampa", "Somente na bateria", &a->lid_suspend, CC_ACT_LID);

    section(c, "NOTIFICAÇÕES");
    row_toggle(c, "Notificações", "Receber alertas de apps", &a->notify, CC_ACT_NOTIFY);
}

/* ================= SEGURANÇA ================= */
static const char *LOCK_OPTS_LOCAL[4] = { "Nunca", "1 minuto", "5 minutos", "15 minutos" };
static void draw_seguranca(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "FIREWALL");
    row_toggle(c, "Firewall", "Bloqueia conexões de entrada não solicitadas", &a->fw, CC_ACT_FW);

    section(c, "BLOQUEIO DE TELA");
    row_dropdown(c, "Bloquear após", LOCK_OPTS_LOCAL, 4, &a->lock_timeout, CC_ACT_LOCK_TIMEOUT);

    section(c, "ACESSO REMOTO");
    row_toggle(c, "SSH", "Acesso remoto por terminal (exige administrador)", &a->ssh, CC_ACT_SSH);

    section(c, "DESENVOLVEDOR");
    row_toggle(c, "Modo desenvolvedor", "Habilita ferramentas extras", &a->devmode, CC_ACT_DEVMODE);
}

/* ================= ATUALIZAÇÕES ================= */
static const char *CHANNEL_OPTS[2] = { "Estável", "Beta" };
static void draw_atualizacoes(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "ATUALIZAÇÕES");
    int n = cc_api_upd_check();
    static char st[48];
    if (n < 0) snprintf(st, sizeof(st), "Indisponível");
    else if (n == 0) snprintf(st, sizeof(st), "Tudo atualizado");
    else snprintf(st, sizeof(st), "%d pacote%s disponíve%s", n, n == 1 ? "" : "s", n == 1 ? "l" : "is");
    row_status(c, "Estado", st, n > 0 ? SWL_AMBER : SWL_CYAN);
    row_button(c, "Verificar agora", SWL_BTN_SECONDARY, CC_ACT_UPD_CHECK, 0, 190);
    row_toggle(c, "Atualização automática", "Baixa e instala sozinho", &a->upd_auto, CC_ACT_UPD_AUTO);

    section(c, "CANAL");
    row_segmented(c, "Canal de atualização", CHANNEL_OPTS, 2, &a->upd_channel, CC_ACT_UPD_CHANNEL);
}

/* ================= DIAGNÓSTICO ================= */
static void draw_diagnostico(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "DIAGNÓSTICO");
    row_button(c, "Verificar sistema", SWL_BTN_SECONDARY, CC_ACT_DIAG, 0, 190);

    static cc_diag_t diags[8];
    int n = cc_api_diag_run(diags, 8);
    for (int i = 0; i < n; i++) {
        double block_top = c->y;
        row_label(c, diags[i].label, diags[i].detail);
        swl_status_dot_draw(c->cr, &a->theme, c->x + c->w - 90, block_top + 6,
                             diags[i].ok ? "OK" : "Atenção", diags[i].ok ? SWL_CYAN : SWL_AMBER);
        if (diags[i].fixable) {
            double bw = 90, bh = 26, bx = c->x + c->w - bw, by = block_top + 26;
            swl_button_draw(c->cr, &a->theme, bx, by, bw, bh, "Corrigir", SWL_BTN_SECONDARY);
            hit_add((hit_t){ (int)bx, (int)by, (int)bw, (int)bh, HIT_ROW, CC_ACT_DIAG_FIX, i, NULL, NULL, NULL });
        }
        c->y = block_top + 58 + GAP;
    }
}

/* ================= GAMING ================= */
static const char *GAME_PROFILE_OPTS[3] = { "Economia", "Balanceado", "Máximo" };
static void draw_gaming(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "GAME MODE");
    row_toggle(c, "Game Mode", "Prioridade total ao jogo", &a->gamemode, CC_ACT_GAMEMODE);
    row_toggle(c, "Ativar ao abrir jogo", "Automático", &a->game_auto, CC_ACT_GAME_AUTO);

    section(c, "DESEMPENHO");
    row_segmented(c, "Perfil", GAME_PROFILE_OPTS, 3, &a->game_profile, CC_ACT_GAME_PROFILE);

    section(c, "OVERLAY");
    row_toggle(c, "Mostrar FPS", "Canto superior", &a->fps_overlay, CC_ACT_FPS);
    row_slider(c, "Transparência do overlay", &a->overlay_alpha, "%d%%", CC_ACT_OVERLAY_ALPHA);
}

/* ================= SOBRE ================= */
static void draw_sobre(cur_t *c) {
    cc_app_t *a = c->a;
    section(c, "SWL OS");
    row_status(c, "Versão", "0.6.0", a->theme.text_dim);
    row_status(c, "Central de Ajustes", "2.0.0", a->theme.text_dim);
    section(c, "BACKUP");
    row_button(c, "Fazer backup agora", SWL_BTN_SECONDARY, CC_ACT_BACKUP_NOW, 0, 180);
    section(c, "AVANÇADO");
    row_button(c, "Restaurar padrões", SWL_BTN_DESTRUCTIVE, CC_ACT_RESET_DEFAULTS, 0, 180);
}

void cc_page_draw(cc_app_t *a, cairo_t *cr, int rx, int ry, int rw, int *out_content_h) {
    nhits = 0;
    cur_t c = { a, cr, rx, rw, ry };
    switch (a->sel) {
    case CC_CAT_REDE: draw_rede(&c); break;
    case CC_CAT_PERSONALIZACAO: draw_person(&c); break;
    case CC_CAT_SISTEMA: draw_sistema(&c); break;
    case CC_CAT_SEGURANCA: draw_seguranca(&c); break;
    case CC_CAT_ATUALIZACOES: draw_atualizacoes(&c); break;
    case CC_CAT_DIAGNOSTICO: draw_diagnostico(&c); break;
    case CC_CAT_GAMING: draw_gaming(&c); break;
    case CC_CAT_SOBRE: draw_sobre(&c); break;
    default: break;
    }
    *out_content_h = c.y - ry;
}

void cc_page_motion(cc_app_t *a, int px, int py) {
    if (a->drag_action) {
        for (int i = 0; i < nhits; i++) {
            if (hits[i].kind == HIT_SLIDER_START && hits[i].action == a->drag_action) {
                double v = (double)(px - hits[i].x) / hits[i].w;
                if (v < 0) v = 0;
                if (v > 1) v = 1;
                *hits[i].dbl_dst = v;
                a->need_redraw = true;
                break;
            }
        }
        return;
    }
    (void)py;
}

void cc_page_press(cc_app_t *a, int px, int py) {
    for (int i = 0; i < nhits; i++) {
        hit_t *h = &hits[i];
        if (px < h->x || px >= h->x + h->w || py < h->y || py >= h->y + h->h) continue;
        switch (h->kind) {
        case HIT_TOGGLE:
            *h->bool_dst = !*h->bool_dst;
            cc_dispatch(a, h->action, *h->bool_dst);
            a->need_redraw = true;
            return;
        case HIT_SEGMENTED: {
            int idx = swl_segmented_hit(h->x, h->y, h->w, h->h, h->arg, px, py);
            if (idx >= 0 && idx != *h->int_dst) {
                *h->int_dst = idx;
                cc_dispatch(a, h->action, idx);
                a->need_redraw = true;
            }
            return;
        }
        case HIT_SLIDER_START:
            a->drag_action = h->action;
            a->need_redraw = true;
            return;
        case HIT_BUTTON:
            cc_dispatch(a, h->action, h->arg);
            a->need_redraw = true;
            return;
        case HIT_ROW:
            if (h->action == CC_ACT_NET_CONNECT) {
                cc_net_t nets[6]; int n = cc_api_wifi_scan(nets, 6);
                snprintf(a->dialog.title, sizeof(a->dialog.title), "Conectar a %s?",
                         (h->arg < n) ? nets[h->arg].ssid : "rede");
                snprintf(a->dialog.body, sizeof(a->dialog.body), "Rede protegida. Digite a senha.");
                a->dialog.ok_label = "Conectar";
                a->dialog.has_field = true;
                memset(&a->dialog.field, 0, sizeof(a->dialog.field));
                a->dialog.field.focused = true;
                a->dialog.field.password = true;
                a->dialog.field.placeholder = "Senha";
                a->dialog.action_ok = CC_ACT_NET_CONNECT;
                a->dialog.action_arg = h->arg;
                a->dialog.open = true;
            } else if (h->action == CC_ACT_DIAG_FIX) {
                cc_diag_t diags[8]; int n = cc_api_diag_run(diags, 8);
                const char *label = h->arg < n ? diags[h->arg].label : "item";
                const char *detail = h->arg < n ? diags[h->arg].detail : "";
                snprintf(a->dialog.title, sizeof(a->dialog.title), "Corrigir \"%s\"?", label);
                snprintf(a->dialog.body, sizeof(a->dialog.body), "%s", detail);
                a->dialog.ok_label = "Aplicar";
                a->dialog.has_field = false;
                a->dialog.action_ok = CC_ACT_DIAG_FIX;
                a->dialog.action_arg = h->arg;
                a->dialog.open = true;
            }
            a->need_redraw = true;
            return;
        case HIT_DROPDOWN_OPEN:
            a->drop.open = true;
            a->drop.x = h->x; a->drop.y = h->y; a->drop.w = h->w; a->drop.h = h->h;
            a->drop.nopts = h->arg;
            a->drop.sel = h->int_dst;
            a->drop.action = h->action;
            /* opções: reaponta pro array certo pela ação (evita
             * guardar ponteiro genérico incompatível) */
            switch (h->action) {
            case CC_ACT_LANG: a->drop.options = cc_lang_opts; break;
            case CC_ACT_LOCK_TIMEOUT: a->drop.options = cc_lock_opts; break;
            default: a->drop.options = NULL; break;
            }
            a->need_redraw = true;
            return;
        default: break;
        }
    }
}

void cc_page_release(cc_app_t *a) {
    if (a->drag_action) {
        cc_dispatch(a, a->drag_action, 0);
        a->drag_action = 0;
    }
}
