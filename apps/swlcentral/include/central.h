#ifndef CENTRAL_H
#define CENTRAL_H

#include <stdbool.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "swl_theme.h"
#include "swl_shapes.h"
#include "swl_controls.h"
#include "swl_overlays.h"
#include "swl_misc.h"
#include "menubar.h"

#define CC_MIN_W 620
#define CC_MIN_H 420
#define CC_SIDEBAR_W 200

/* ---- catálogo de categorias (sidebar) ---- */
typedef enum {
    CC_CAT_REDE, CC_CAT_PERSONALIZACAO, CC_CAT_SISTEMA, CC_CAT_SEGURANCA,
    CC_CAT_ATUALIZACOES, CC_CAT_DIAGNOSTICO, CC_CAT_GAMING, CC_CAT_SOBRE,
    CC_CAT_COUNT,
} cc_cat_t;

/* ---- dados vindos do backend (api.c) ---- */
typedef struct { const char *ssid; const char *meta; int bars; bool locked; bool connected; } cc_net_t;
typedef struct { const char *label; const char *detail; bool ok; bool fixable; } cc_diag_t;

/* ---- ações (dispatch único, ver api.c) ---- */
typedef enum {
    CC_ACT_NONE = 0,
    CC_ACT_WIFI, CC_ACT_BT, CC_ACT_ETH, CC_ACT_SCAN_WIFI, CC_ACT_NET_CONNECT,
    CC_ACT_THEME, CC_ACT_ACCENT, CC_ACT_TRANSP, CC_ACT_SCALE, CC_ACT_REDUCE_MOTION, CC_ACT_LANG,
    CC_ACT_POWER, CC_ACT_BRIGHT, CC_ACT_LID, CC_ACT_NOTIFY, CC_ACT_HOSTNAME,
    CC_ACT_FW, CC_ACT_SSH, CC_ACT_DEVMODE, CC_ACT_LOCK_TIMEOUT,
    CC_ACT_UPD_CHECK, CC_ACT_UPD_AUTO, CC_ACT_UPD_CHANNEL,
    CC_ACT_DIAG, CC_ACT_DIAG_FIX,
    CC_ACT_GAMEMODE, CC_ACT_GAME_AUTO, CC_ACT_GAME_PROFILE, CC_ACT_FPS, CC_ACT_OVERLAY_ALPHA,
    CC_ACT_BACKUP_NOW, CC_ACT_RESET_DEFAULTS,
} cc_action_t;

/* ---- widget de campo de diálogo (senha etc.) ---- */
typedef struct {
    bool focused;
    char buf[128];
    const char *placeholder;
    bool password;
} cc_field_t;

typedef struct {
    bool open;
    char title[96], body[160];
    const char *ok_label;
    bool has_field;
    cc_field_t field;
    int action_ok, action_arg;
} cc_dialog_t;

/* dropdown genérico: só um pode estar aberto por vez */
typedef struct {
    bool open;
    int id;               /* identifica qual dropdown (ação associada) */
    double x, y, w, h;     /* geometria do botão fechado (coords de conteúdo, sem scroll) */
    const char *const *options;
    int nopts;
    int *sel;              /* ponteiro pro valor no app — dropdown escreve direto */
    int hover;
    int action;
} cc_drop_t;

typedef struct cc_app {
    /* wayland */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct wl_seat *seat;
    struct wl_surface *surface;
    struct xdg_surface *xsurface;
    struct xdg_toplevel *toplevel;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *xkb_state;
    struct wl_buffer *buffer;
    void *buffer_data;
    size_t buffer_size;
    cairo_surface_t *csurf;
    cairo_t *cr;

    int width, height;
    bool running, configured, need_redraw;
    int px, py;

    swl_theme_t theme;
    swl_menubar *menubar;
    char search[96];
    bool search_focused;
    swl_search_hit search_hits[6];
    cc_cat_t search_cats[6];
    int search_nhits;

    cc_cat_t sel;
    double scroll, content_h, view_h;
    bool scrollbar_drag;
    bool advanced;

    cc_dialog_t dialog;
    cc_drop_t drop;
    int drag_action;

    /* preferências em memória (espelham api.c, que é quem persiste) */
    bool wifi, bt, eth, notify, gamemode, game_auto, fps_overlay, fw, ssh, devmode,
         upd_auto, reduce_motion, lid_suspend;
    double bright, transp, scale, overlay_alpha;
    int theme_mode;      /* 0 claro, 1 escuro, 2 auto */
    int power_profile;   /* 0..2 */
    int game_profile;    /* 0..2 */
    int lang;             /* índice em cc_lang_opts */
    int upd_channel;      /* 0 estável, 1 beta */
    int lock_timeout;     /* índice em cc_lock_opts */
    char hostname[64];
} cc_app_t;

/* ---- API (backend real, api.c) ---- */
bool cc_api_wifi_get(bool *on);
bool cc_api_wifi_set(bool on);
int cc_api_wifi_scan(cc_net_t *out, int cap);
bool cc_api_net_connect(int idx, const char *password);
bool cc_api_bt_get(bool *on);
bool cc_api_bt_set(bool on);
bool cc_api_eth_get(bool *on, char *iface, size_t ifacecap);
bool cc_api_eth_set(bool on);
bool cc_api_net_status(char *ssid, size_t ssidcap, char *ip, size_t ipcap, int *signal_pct);

double cc_api_bright_get(void);
bool cc_api_bright_set(double v);
int cc_api_power_get(void);
bool cc_api_power_set(int p);

int cc_api_diag_run(cc_diag_t *out, int cap);
bool cc_api_diag_fix(int idx);

bool cc_api_fw_get(bool *on);
bool cc_api_fw_set(bool on);
bool cc_api_ssh_get(bool *on);
bool cc_api_ssh_set(bool on);

int cc_api_upd_check(void);
bool cc_api_backup_now(void);
bool cc_api_reset_defaults(cc_app_t *a);

bool cc_api_conf_load(cc_app_t *a);
bool cc_api_conf_save(cc_app_t *a);
void cc_api_log(const char *component, const char *setting, const char *before, const char *after, bool ok);

void cc_dispatch(cc_app_t *a, int action, int arg);

/* ---- UI (ui.c / pages.c) ---- */
void cc_draw_all(cc_app_t *a);
void cc_contentgeom(cc_app_t *a, int *sx, int *sy, int *sw, int *rx, int *ry, int *rw, int *bot);
void cc_page_motion(cc_app_t *a, int px, int py);
void cc_page_press(cc_app_t *a, int px, int py);
void cc_page_release(cc_app_t *a);
void cc_refresh_search(cc_app_t *a);
bool cc_field_key(cc_field_t *f, struct xkb_state *xkb, uint32_t keycode, xkb_keysym_t sym);
void cc_draw_small_caption(cairo_t *cr, const char *text, double x, double y, swl_color_t color);
void cc_text_wh(const char *text, double size, int *w, int *h);

extern const char *const cc_cat_names[CC_CAT_COUNT];
extern const char *const cc_lang_opts[3];
extern const char *const cc_lock_opts[4];

#endif /* CENTRAL_H */
