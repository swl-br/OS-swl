/*
 * term.c — grid de células + parser ANSI/VT100 do TSWL.
 *
 * Parser: máquina de estados byte a byte (GROUND → ESC → CSI → OSC).
 * Decodificação UTF-8 incremental embutida (1-4 bytes → codepoint).
 * Nenhuma alocação acontece em tswl_term_feed — toda a memória é
 * pré-alocada na criação/resize.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "term.h"

#define MAX_CSI_PARAMS 16

enum parser_state {
    ST_GROUND,
    ST_ESC,       /* recebeu ESC, esperando o tipo da sequência */
    ST_CSI,       /* dentro de CSI ... final-byte */
    ST_OSC_STR,   /* dentro de OSC, consumindo até BEL ou ST */
    ST_OSC_ESC,   /* OSC e recebeu ESC (possível ST) */
};

struct tswl_term {
    int cols, rows;
    tswl_cell *grid;          /* cols*rows, tela visível */
    tswl_cell *main_save;     /* cópia da tela principal enquanto alt-screen */
    tswl_cell *back;          /* ring buffer: TSWL_SCROLLBACK * cols */
    int back_head;            /* próxima posição de escrita no ring */
    int back_count;           /* linhas válidas no ring (<= TSWL_SCROLLBACK) */
    uint8_t *dirty;           /* 1 byte por linha da tela */

    int cx, cy;               /* cursor */
    int saved_cx, saved_cy;
    bool alt_screen;          /* CSI ? 1049 h/l — buffer alternativo */
    uint16_t cur_fg, cur_bg;
    uint8_t cur_attrs;
    bool cursor_visible;
    bool app_cursor;          /* DECCKM: CSI ? 1 h/l (R-14) */
    bool bracketed_paste;     /* CSI ? 2004 h/l */
    int scroll_top, scroll_bot;  /* região de scroll (linhas, inclusivo) */

    int scroll_offset;        /* scrollback visual (0 = fim) */

    /* T5 selecao (coords de tela) */
    bool sel_active;
    int sel_c0, sel_r0, sel_c1, sel_r1;

    enum parser_state state;
    int csi_params[MAX_CSI_PARAMS];
    int csi_nparams;
    bool csi_private;         /* '?' logo após '[' */
    char csi_intermed;        /* intermediate (ex: '!' em CSI !p) */
    uint32_t utf8_cp;         /* codepoint parcial */
    int utf8_left;            /* bytes restantes do caractere atual */
    bool changed;

    /* OSC 0/2 titulo + OSC 52 clipboard */
    char osc_buf[1024];
    int osc_len;
    char window_title[256];
    bool title_pending;
    char *clip_pending;
    size_t clip_pending_len;
    bool clip_pending_set;

    /* visual bell (BEL em ground; OSC usa 0x07 so como terminador) */
    bool bell_pending;
};

static void osc_finish(tswl_term *t);
static char *b64_decode(const char *in, size_t inlen, size_t *outlen);

static tswl_cell blank_cell(uint16_t bg) {
    tswl_cell c = { 0, TSWL_COL_DEFAULT_FG, bg, 0 };
    return c;
}

tswl_term *tswl_term_new(int cols, int rows) {
    tswl_term *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->grid = malloc((size_t)cols * rows * sizeof(tswl_cell));
    t->main_save = malloc((size_t)cols * rows * sizeof(tswl_cell));
    t->back = malloc((size_t)TSWL_SCROLLBACK * cols * sizeof(tswl_cell));
    t->dirty = malloc((size_t)rows);
    if (!t->grid || !t->main_save || !t->back || !t->dirty) {
        tswl_term_free(t);
        return NULL;
    }
    t->cols = cols;
    t->rows = rows;
    t->cur_fg = TSWL_COL_DEFAULT_FG;
    t->cur_bg = TSWL_COL_DEFAULT_BG;
    t->cursor_visible = true;
    t->scroll_top = 0;
    t->scroll_bot = rows - 1;
    tswl_cell b = blank_cell(TSWL_COL_DEFAULT_BG);
    for (int i = 0; i < cols * rows; i++) {
        t->grid[i] = b;
        t->main_save[i] = b;
    }
    memset(t->dirty, 1, (size_t)rows);
    t->osc_len = 0;
    t->window_title[0] = '\0';
    t->title_pending = false;
    t->clip_pending = NULL;
    t->clip_pending_len = 0;
    t->clip_pending_set = false;
    t->bell_pending = false;
    return t;
}

void tswl_term_free(tswl_term *t) {
    if (!t) return;
    free(t->clip_pending);
    free(t->grid);
    free(t->main_save);
    free(t->back);
    free(t->dirty);
    free(t);
}

int tswl_term_cols(const tswl_term *t) { return t->cols; }
int tswl_term_rows(const tswl_term *t) { return t->rows; }
int tswl_term_cursor_x(const tswl_term *t) { return t->cx; }
int tswl_term_cursor_y(const tswl_term *t) { return t->cy; }
bool tswl_term_cursor_visible(const tswl_term *t) { return t->cursor_visible; }
bool tswl_term_app_cursor(const tswl_term *t) { return t->app_cursor; }
bool tswl_term_bracketed_paste(const tswl_term *t) { return t && t->bracketed_paste; }
int tswl_term_scroll_offset(const tswl_term *t) { return t->scroll_offset; }

const tswl_cell *tswl_term_cell(const tswl_term *t, int col, int row) {
    return &t->grid[(size_t)row * t->cols + col];
}

/* célula visível considerando scrollback: row 0 é o topo da TELA;
 * quando scroll_offset > 0, as primeiras linhas vêm do ring buffer. */
const tswl_cell *tswl_term_scrollback_cell(const tswl_term *t, int col, int row) {
    int from_back = t->scroll_offset - row;  /* >0: linha vem do histórico */
    if (from_back <= 0) {
        return tswl_term_cell(t, col, row - t->scroll_offset);
    }
    int idx = t->back_count - from_back;     /* linha relativa no ring */
    if (idx < 0) {
        static tswl_cell empty = { 0, TSWL_COL_DEFAULT_FG, TSWL_COL_DEFAULT_BG, 0 };
        return &empty;
    }
    int ring = (t->back_head - t->back_count + idx + TSWL_SCROLLBACK) % TSWL_SCROLLBACK;
    return &t->back[(size_t)ring * t->cols + col];
}

bool tswl_term_row_dirty(tswl_term *t, int row) { return t->dirty[row] != 0; }

static void mark_all_dirty(tswl_term *t);

void tswl_term_clear_dirty(tswl_term *t) {
    memset(t->dirty, 0, (size_t)t->rows);
}

void tswl_term_clear_selection(tswl_term *t) {
    if (!t) return;
    if (t->sel_active) {
        t->sel_active = false;
        mark_all_dirty(t);
        t->changed = true;
    }
}




void tswl_term_select_all(tswl_term *t)
{
    if (!t || t->cols < 1 || t->rows < 1) return;
    tswl_term_set_selection(t, 0, 0, t->cols - 1, t->rows - 1);
}

void tswl_term_select_line(tswl_term *t, int row)
{
    if (!t) return;
    if (row < 0 || row >= t->rows) return;
    int c1 = t->cols - 1;
    while (c1 > 0) {
        const tswl_cell *cell = tswl_term_scrollback_cell(t, c1, row);
        if (cell->ch != 0 && cell->ch != ' ')
            break;
        c1--;
    }
    tswl_term_set_selection(t, 0, row, c1, row);
}

void tswl_term_select_word(tswl_term *t, int col, int row)
{
    if (!t) return;
    if (col < 0 || row < 0 || col >= t->cols || row >= t->rows) return;
    const tswl_cell *cell = tswl_term_scrollback_cell(t, col, row);
    uint32_t ch = cell->ch;
    if (ch == 0 || ch == ' ') {
        tswl_term_set_selection(t, col, row, col, row);
        return;
    }
    int is_word = (ch < 128 && ((ch >= '0' && ch <= '9')
        || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'));
    int c0 = col, c1 = col;
    if (is_word) {
        while (c0 > 0) {
            const tswl_cell *L = tswl_term_scrollback_cell(t, c0 - 1, row);
            uint32_t x = L->ch;
            if (!(x < 128 && ((x >= '0' && x <= '9')
                || (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_')))
                break;
            c0--;
        }
        while (c1 + 1 < t->cols) {
            const tswl_cell *R = tswl_term_scrollback_cell(t, c1 + 1, row);
            uint32_t x = R->ch;
            if (!(x < 128 && ((x >= '0' && x <= '9')
                || (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_')))
                break;
            c1++;
        }
    }
    tswl_term_set_selection(t, c0, row, c1, row);
}

void tswl_term_set_selection(tswl_term *t, int c0, int r0, int c1, int r1) {
    if (!t) return;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 < 0) c1 = 0;
    if (r1 < 0) r1 = 0;
    if (c0 >= t->cols) c0 = t->cols - 1;
    if (c1 >= t->cols) c1 = t->cols - 1;
    if (r0 >= t->rows) r0 = t->rows - 1;
    if (r1 >= t->rows) r1 = t->rows - 1;
    t->sel_c0 = c0; t->sel_r0 = r0;
    t->sel_c1 = c1; t->sel_r1 = r1;
    t->sel_active = true;
    mark_all_dirty(t);
    t->changed = true;
}

bool tswl_term_has_selection(const tswl_term *t) {
    return t && t->sel_active;
}

static void sel_norm(const tswl_term *t, int *sc, int *sr, int *ec, int *er) {
    int a = t->sel_r0 * t->cols + t->sel_c0;
    int b = t->sel_r1 * t->cols + t->sel_c1;
    if (a <= b) {
        *sc = t->sel_c0; *sr = t->sel_r0;
        *ec = t->sel_c1; *er = t->sel_r1;
    } else {
        *sc = t->sel_c1; *sr = t->sel_r1;
        *ec = t->sel_c0; *er = t->sel_r0;
    }
}

bool tswl_term_cell_selected(const tswl_term *t, int col, int row) {
    if (!t || !t->sel_active) return false;
    if (col < 0 || row < 0 || col >= t->cols || row >= t->rows) return false;
    int sc, sr, ec, er;
    sel_norm(t, &sc, &sr, &ec, &er);
    int pos = row * t->cols + col;
    int a = sr * t->cols + sc;
    int b = er * t->cols + ec;
    return pos >= a && pos <= b;
}

char *tswl_term_selection_text(const tswl_term *t) {
    if (!t || !t->sel_active) return NULL;
    int sc, sr, ec, er;
    sel_norm(t, &sc, &sr, &ec, &er);
    size_t cap = (size_t)(er - sr + 1) * ((size_t)t->cols * 4 + 1) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    for (int r = sr; r <= er; r++) {
        int x0 = (r == sr) ? sc : 0;
        int x1 = (r == er) ? ec : (t->cols - 1);
        int end = x1;
        while (end >= x0) {
            const tswl_cell *cell = tswl_term_scrollback_cell(t, end, r);
            if (cell->ch != 0 && cell->ch != ' ') break;
            end--;
        }
        for (int x = x0; x <= end; x++) {
            const tswl_cell *cell = tswl_term_scrollback_cell(t, x, r);
            uint32_t cp = cell->ch ? cell->ch : (uint32_t)' ';
            if (cp < 0x80) {
                if (n + 1 >= cap) break;
                out[n++] = (char)cp;
            } else if (cp < 0x800) {
                if (n + 2 >= cap) break;
                out[n++] = (char)(0xC0 | (cp >> 6));
                out[n++] = (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                if (n + 3 >= cap) break;
                out[n++] = (char)(0xE0 | (cp >> 12));
                out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[n++] = (char)(0x80 | (cp & 0x3F));
            } else {
                if (n + 4 >= cap) break;
                out[n++] = (char)(0xF0 | (cp >> 18));
                out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[n++] = (char)(0x80 | (cp & 0x3F));
            }
        }
        if (r < er) {
            if (n + 1 >= cap) break;
            out[n++] = '\n';
        }
    }
    out[n] = 0;
    return out;
}


static void mark_all_dirty(tswl_term *t) {
    memset(t->dirty, 1, (size_t)t->rows);
}

static void dirty_row(tswl_term *t, int row) {
    if (row >= 0 && row < t->rows) {
        t->dirty[row] = 1;
        t->changed = true;
    }
}

static void clamp_cursor(tswl_term *t) {
    if (t->cx < 0) t->cx = 0;
    if (t->cy < 0) t->cy = 0;
    if (t->cx >= t->cols) t->cx = t->cols - 1;
    if (t->cy >= t->rows) t->cy = t->rows - 1;
}

/* empurra a linha do topo da região de scroll pro ring de histórico
 * (só quando a região é a tela inteira, senão a linha morre). */
static void scroll_up(tswl_term *t, int n) {
    if (n <= 0) return;
    if (t->scroll_top > t->scroll_bot) return;  /* região inválida: no-op */
    int span = t->scroll_bot - t->scroll_top;   /* linhas a mover (>= 0) */
    for (int k = 0; k < n; k++) {
        if (t->scroll_top == 0 && t->scroll_bot == t->rows - 1 && !t->alt_screen) {
            memcpy(&t->back[(size_t)t->back_head * t->cols],
                   &t->grid[0], (size_t)t->cols * sizeof(tswl_cell));
            t->back_head = (t->back_head + 1) % TSWL_SCROLLBACK;
            if (t->back_count < TSWL_SCROLLBACK) t->back_count++;
        }
        if (span > 0) {
            memmove(&t->grid[(size_t)t->scroll_top * t->cols],
                    &t->grid[(size_t)(t->scroll_top + 1) * t->cols],
                    (size_t)span * t->cols * sizeof(tswl_cell));
        }
        tswl_cell b = blank_cell(t->cur_bg);
        for (int x = 0; x < t->cols; x++) {
            t->grid[(size_t)t->scroll_bot * t->cols + x] = b;
        }
    }
    for (int r = t->scroll_top; r <= t->scroll_bot; r++) dirty_row(t, r);
    /* scroll real joga a viewport pro fim */
    t->scroll_offset = 0;
}

static void scroll_down(tswl_term *t, int n) {
    if (n <= 0) return;
    if (t->scroll_top > t->scroll_bot) return;  /* região inválida: no-op */
    int span = t->scroll_bot - t->scroll_top;
    for (int k = 0; k < n; k++) {
        if (span > 0) {
            memmove(&t->grid[(size_t)(t->scroll_top + 1) * t->cols],
                    &t->grid[(size_t)t->scroll_top * t->cols],
                    (size_t)span * t->cols * sizeof(tswl_cell));
        }
        tswl_cell b = blank_cell(t->cur_bg);
        for (int x = 0; x < t->cols; x++) {
            t->grid[(size_t)t->scroll_top * t->cols + x] = b;
        }
    }
    for (int r = t->scroll_top; r <= t->scroll_bot; r++) dirty_row(t, r);
}

static void newline(tswl_term *t) {
    if (t->cy == t->scroll_bot) {
        scroll_up(t, 1);
    } else if (t->cy < t->rows - 1) {
        t->cy++;
    }
}

static void put_char(tswl_term *t, uint32_t cp) {
    if (t->cx >= t->cols) {
        t->cx = 0;
        newline(t);
    }
    tswl_cell *c = &t->grid[(size_t)t->cy * t->cols + t->cx];
    c->ch = cp;
    c->fg = t->cur_fg;
    c->bg = t->cur_bg;
    c->attrs = t->cur_attrs;
    dirty_row(t, t->cy);
    t->cx++;
}

static void erase_range(tswl_term *t, int row, int x0, int x1) {
    tswl_cell b = blank_cell(t->cur_bg);
    for (int x = x0; x <= x1 && x < t->cols; x++) {
        t->grid[(size_t)row * t->cols + x] = b;
    }
    dirty_row(t, row);
}

/* Limite seguro para parâmetros CSI numéricos. Terminais reais clampam
 * contagens grandes (evita overflow de int e laços de milhões de
 * memmove). 9999 cobre qualquer tela razoável e evita UB. */
#define CSI_PARAM_MAX 9999

static int param(tswl_term *t, int i, int def) {
    if (i >= t->csi_nparams || t->csi_params[i] == 0) return def;
    int v = t->csi_params[i];
    if (v < 0) return def;          /* overflow → valor negativo: ignora */
    if (v > CSI_PARAM_MAX) return CSI_PARAM_MAX;
    return v;
}


/* Aproxima RGB 24-bit no cubo 256 (xterm) — truecolor leve sem expandir a celula. */
static int rgb_to_256(int r, int g, int b)
{
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    int avg = (r + g + b) / 3;
    int dr = r > avg ? r - avg : avg - r;
    int dg = g > avg ? g - avg : avg - g;
    int db = b > avg ? b - avg : avg - b;
    if (dr < 8 && dg < 8 && db < 8) {
        if (avg < 8) return 16;
        if (avg > 248) return 231;
        return 232 + (avg - 8) / 10;
    }
    int ri = (r * 5 + 127) / 255;
    int gi = (g * 5 + 127) / 255;
    int bi = (b * 5 + 127) / 255;
    if (ri > 5) ri = 5;
    if (gi > 5) gi = 5;
    if (bi > 5) bi = 5;
    return 16 + 36 * ri + 6 * gi + bi;
}

static void csi_sgr(tswl_term *t) {
    if (t->csi_nparams == 0) {  /* CSI m = reset */
        t->cur_fg = TSWL_COL_DEFAULT_FG;
        t->cur_bg = TSWL_COL_DEFAULT_BG;
        t->cur_attrs = 0;
        return;
    }
    for (int i = 0; i < t->csi_nparams; i++) {
        int p = t->csi_params[i];
        if (p == 0) {
            t->cur_fg = TSWL_COL_DEFAULT_FG;
            t->cur_bg = TSWL_COL_DEFAULT_BG;
            t->cur_attrs = 0;
        } else if (p == 1) t->cur_attrs |= TSWL_ATTR_BOLD;
        else if (p == 2) t->cur_attrs |= TSWL_ATTR_DIM;
        else if (p == 3) t->cur_attrs |= TSWL_ATTR_ITALIC;
        else if (p == 4) t->cur_attrs |= TSWL_ATTR_UNDERLINE;
        else if (p == 7) t->cur_attrs |= TSWL_ATTR_REVERSE;
        else if (p == 22) t->cur_attrs &= ~(TSWL_ATTR_BOLD | TSWL_ATTR_DIM);
        else if (p == 23) t->cur_attrs &= ~TSWL_ATTR_ITALIC;
        else if (p == 24) t->cur_attrs &= ~TSWL_ATTR_UNDERLINE;
        else if (p == 27) t->cur_attrs &= ~TSWL_ATTR_REVERSE;
        else if (p >= 30 && p <= 37) t->cur_fg = (uint16_t)(p - 30);
        else if (p == 39) t->cur_fg = TSWL_COL_DEFAULT_FG;
        else if (p >= 40 && p <= 47) t->cur_bg = (uint16_t)(p - 40);
        else if (p == 49) t->cur_bg = TSWL_COL_DEFAULT_BG;
        else if (p >= 90 && p <= 97) t->cur_fg = (uint16_t)(p - 90 + 8);
        else if (p >= 100 && p <= 107) t->cur_bg = (uint16_t)(p - 100 + 8);
        /* 256-color: CSI 38;5;n m / 48;5;n m */
        else if (p == 38 && i + 2 < t->csi_nparams && t->csi_params[i + 1] == 5) {
            int n = t->csi_params[i + 2];
            if (n < 0) n = 0;
            if (n > 255) n = 255;
            t->cur_fg = (uint16_t)n;
            i += 2;
        } else if (p == 48 && i + 2 < t->csi_nparams && t->csi_params[i + 1] == 5) {
            int n = t->csi_params[i + 2];
            if (n < 0) n = 0;
            if (n > 255) n = 255;
            t->cur_bg = (uint16_t)n;
            i += 2;
        } else if (p == 38 && i + 4 < t->csi_nparams && t->csi_params[i + 1] == 2) {
            /* truecolor 38;2;r;g;b → quantiza pro cubo 256 */
            int r = t->csi_params[i + 2];
            int g = t->csi_params[i + 3];
            int b = t->csi_params[i + 4];
            t->cur_fg = (uint16_t)rgb_to_256(r, g, b);
            i += 4;
        } else if (p == 48 && i + 4 < t->csi_nparams && t->csi_params[i + 1] == 2) {
            int r = t->csi_params[i + 2];
            int g = t->csi_params[i + 3];
            int b = t->csi_params[i + 4];
            t->cur_bg = (uint16_t)rgb_to_256(r, g, b);
            i += 4;
        }
    }
}

static void csi_dispatch(tswl_term *t, char final) {
    int n;
    switch (final) {
    case 'A': t->cy -= param(t, 0, 1); clamp_cursor(t); break;
    case 'B': t->cy += param(t, 0, 1); clamp_cursor(t); break;
    case 'C': t->cx += param(t, 0, 1); clamp_cursor(t); break;
    case 'D': t->cx -= param(t, 0, 1); clamp_cursor(t); break;
    case 'E': t->cy += param(t, 0, 1); t->cx = 0; clamp_cursor(t); break;
    case 'F': t->cy -= param(t, 0, 1); t->cx = 0; clamp_cursor(t); break;
    case 'G': t->cx = param(t, 0, 1) - 1; clamp_cursor(t); break;
    case 'H': case 'f':
        t->cy = param(t, 0, 1) - 1;
        t->cx = param(t, 1, 1) - 1;
        clamp_cursor(t);
        break;
    case 'J':  /* erase in display */
        n = param(t, 0, 0);
        if (n == 0) {
            erase_range(t, t->cy, t->cx, t->cols - 1);
            for (int r = t->cy + 1; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
        } else if (n == 1) {
            erase_range(t, t->cy, 0, t->cx);
            for (int r = 0; r < t->cy; r++) erase_range(t, r, 0, t->cols - 1);
        } else if (n == 2) {
            for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
        } else if (n == 3) {
            /* CSI 3 J — limpa tela + scrollback (xterm) */
            for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
            if (t->back) {
                tswl_cell bb = blank_cell(t->cur_bg);
                for (int i = 0; i < TSWL_SCROLLBACK * t->cols; i++)
                    t->back[i] = bb;
            }
            t->back_count = 0;
            t->back_head = 0;
            t->scroll_offset = 0;
        }
        break;
    case 'K':  /* erase in line */
        n = param(t, 0, 0);
        if (n == 0) erase_range(t, t->cy, t->cx, t->cols - 1);
        else if (n == 1) erase_range(t, t->cy, 0, t->cx);
        else erase_range(t, t->cy, 0, t->cols - 1);
        break;
    case 'L': {  /* insert lines */
        int cnt = param(t, 0, 1);
        int region = t->scroll_bot - t->scroll_top + 1;
        if (region < 1) region = 1;
        if (cnt > region) cnt = region;
        if (t->cy >= t->scroll_top && t->cy <= t->scroll_bot) {
            int save_top = t->scroll_top;
            t->scroll_top = t->cy;
            scroll_down(t, cnt);
            t->scroll_top = save_top;
        }
        break;
    }
    case 'M': {  /* delete lines */
        int cnt = param(t, 0, 1);
        int region = t->scroll_bot - t->scroll_top + 1;
        if (region < 1) region = 1;
        if (cnt > region) cnt = region;
        if (t->cy >= t->scroll_top && t->cy <= t->scroll_bot) {
            int save_top = t->scroll_top;
            t->scroll_top = t->cy;
            scroll_up(t, cnt);
            t->scroll_top = save_top;
        }
        break;
    }
    case 'P': {  /* delete chars */
        int cnt = param(t, 0, 1);
        int line_len = t->cols - t->cx;
        if (cnt > line_len) cnt = line_len;
        memmove(&t->grid[(size_t)t->cy * t->cols + t->cx],
                &t->grid[(size_t)t->cy * t->cols + t->cx + cnt],
                (size_t)(line_len - cnt) * sizeof(tswl_cell));
        erase_range(t, t->cy, t->cols - cnt, t->cols - 1);
        break;
    }
    case 'S': {
        int cnt = param(t, 0, 1);
        int region = t->scroll_bot - t->scroll_top + 1;
        if (region < 1) region = 1;
        if (cnt > region) cnt = region;
        scroll_up(t, cnt);
        break;
    }
    case 'T': {
        int cnt = param(t, 0, 1);
        int region = t->scroll_bot - t->scroll_top + 1;
        if (region < 1) region = 1;
        if (cnt > region) cnt = region;
        scroll_down(t, cnt);
        break;
    }
    case 'X': {  /* erase chars */
        int cnt = param(t, 0, 1);
        erase_range(t, t->cy, t->cx, t->cx + cnt - 1);
        break;
    }
    case 'd': t->cy = param(t, 0, 1) - 1; clamp_cursor(t); break;
    case 'm': csi_sgr(t); break;

    case 'p':
        if (t->csi_intermed == '!') {
            /* DECSTR soft reset: attrs, scroll region, modes comuns */
            t->cur_fg = TSWL_COL_DEFAULT_FG;
            t->cur_bg = TSWL_COL_DEFAULT_BG;
            t->cur_attrs = 0;
            t->scroll_top = 0;
            t->scroll_bot = t->rows - 1;
            t->app_cursor = false;
            t->bracketed_paste = false;
            t->cursor_visible = true;
            t->changed = true;
        }
        break;
    case 'r':  /* set scroll region */
        /* CSI Pt ; Pb r — região inclusiva. Região inválida
         * (top > bot ou fora dos limites) → tela inteira.
         * Sem este clamp, ESC[9999;1r + CSI S gera memmove com
         * tamanho negativo (size_t gigante) → corrupção de memória. */
        t->scroll_top = param(t, 0, 1) - 1;
        t->scroll_bot = param(t, 1, t->rows) - 1;
        if (t->scroll_top < 0) t->scroll_top = 0;
        if (t->scroll_top >= t->rows) t->scroll_top = t->rows - 1;
        if (t->scroll_bot < 0) t->scroll_bot = 0;
        if (t->scroll_bot >= t->rows) t->scroll_bot = t->rows - 1;
        if (t->scroll_top > t->scroll_bot) {
            t->scroll_top = 0;
            t->scroll_bot = t->rows - 1;
        }
        t->cx = 0; t->cy = 0;
        break;
    case 's': t->saved_cx = t->cx; t->saved_cy = t->cy; break;
    case 'u': t->cx = t->saved_cx; t->cy = t->saved_cy; clamp_cursor(t); break;
    case 'h': case 'l': {  /* set/reset mode (só o que importa) */
        bool set = (final == 'h');
        for (int i = 0; i < t->csi_nparams; i++) {
            if (t->csi_private && t->csi_params[i] == 25) {
                t->cursor_visible = set;
                t->changed = true;
            } else if (t->csi_private && t->csi_params[i] == 1) {
                /* DECCKM: application cursor keys (R-14) */
                t->app_cursor = set;
            } else if (t->csi_private && t->csi_params[i] == 2004) {
                /* bracketed paste */
                t->bracketed_paste = set;
            } else if (t->csi_private && t->csi_params[i] == 1049) {
                /* Alt screen real: salva a tela principal em main_save,
                 * limpa grid para o app (vim/htop); ao sair restaura.
                 * Em alt, scroll não alimenta o scrollback. */
                if (set && !t->alt_screen) {
                    memcpy(t->main_save, t->grid,
                           (size_t)t->cols * t->rows * sizeof(tswl_cell));
                    t->saved_cx = t->cx;
                    t->saved_cy = t->cy;
                    for (int r = 0; r < t->rows; r++)
                        erase_range(t, r, 0, t->cols - 1);
                    t->cx = 0;
                    t->cy = 0;
                    t->scroll_offset = 0;
                    t->alt_screen = true;
                    t->changed = true;
                } else if (!set && t->alt_screen) {
                    memcpy(t->grid, t->main_save,
                           (size_t)t->cols * t->rows * sizeof(tswl_cell));
                    t->cx = t->saved_cx;
                    t->cy = t->saved_cy;
                    clamp_cursor(t);
                    t->alt_screen = false;
                    t->changed = true;
                    mark_all_dirty(t);
                }
            }
        }
        break;
    }
    default:
        break;  /* sequência desconhecida: ignora (filosofia: nunca quebrar) */
    }
}

/* UTF-8 incremental: retorna codepoint completo (>0) ou 0 se incompleto.
 * Bytes inválidos retornam U+FFFD. */
static uint32_t utf8_step(tswl_term *t, unsigned char b, bool *ready) {
    *ready = false;
    if (t->utf8_left == 0) {
        if (b < 0x80) { *ready = true; return b; }
        if ((b & 0xE0) == 0xC0) { t->utf8_cp = b & 0x1F; t->utf8_left = 1; return 0; }
        if ((b & 0xF0) == 0xE0) { t->utf8_cp = b & 0x0F; t->utf8_left = 2; return 0; }
        if ((b & 0xF8) == 0xF0) { t->utf8_cp = b & 0x07; t->utf8_left = 3; return 0; }
        *ready = true;
        return 0xFFFD;
    }
    if ((b & 0xC0) != 0x80) {  /* continuação inválida: aborta caractere */
        t->utf8_left = 0;
        *ready = true;
        return 0xFFFD;
    }
    t->utf8_cp = (t->utf8_cp << 6) | (b & 0x3F);
    if (--t->utf8_left == 0) { *ready = true; return t->utf8_cp; }
    return 0;
}

static void ground_byte(tswl_term *t, unsigned char b) {
    bool ready;
    uint32_t cp = utf8_step(t, b, &ready);
    if (!ready) return;

    switch (cp) {
    case '\r': t->cx = 0; break;
    case '\n': case '\v': case '\f': newline(t); break;
    case '\b': if (t->cx > 0) t->cx--; break;
    case 0x07:  /* BEL — visual bell (nao e terminador OSC aqui) */
        t->bell_pending = true;
        t->changed = true;
        break;
    case '\t': {
        int next = (t->cx + 8) & ~7;
        tswl_cell fill = blank_cell(t->cur_bg);
        fill.fg = t->cur_fg;
        fill.attrs = t->cur_attrs;
        while (t->cx < next && t->cx < t->cols) {
            t->grid[(size_t)t->cy * t->cols + t->cx++] = fill;
        }
        dirty_row(t, t->cy);
        break;
    }
    default:
        if (cp >= 0x20 && cp != 0x7F) {
            put_char(t, cp);
            t->scroll_offset = 0;  /* saída nova joga a viewport pro fim */
        }
        break;
    }
}

bool tswl_term_feed(tswl_term *t, const char *data, size_t len) {
    t->changed = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char b = (unsigned char)data[i];

        if (t->state == ST_OSC_STR || t->state == ST_OSC_ESC) {
            /* OSC ... ate BEL ou ST — captura titulo (0/2) */
            if (t->state == ST_OSC_ESC && b == '\\') {
                osc_finish(t);
                t->state = ST_GROUND;
                continue;
            }
            if (b == 0x1B) {
                t->state = ST_OSC_ESC;
                continue;
            }
            if (b == 0x07) {
                osc_finish(t);
                t->state = ST_GROUND;
                continue;
            } else if (t->state == ST_OSC_ESC) {
                t->state = ST_OSC_STR;
            } else if (t->osc_len < (int)sizeof(t->osc_buf) - 1 && b >= 0x20) {
                t->osc_buf[t->osc_len++] = (char)b;
            }
            continue;
        }

        if (t->state == ST_ESC) {
            t->state = ST_GROUND;
            switch (b) {
            case '[': t->state = ST_CSI; t->csi_nparams = 0;
                      t->csi_private = false;
                t->csi_intermed = 0;
                      memset(t->csi_params, 0, sizeof(t->csi_params));
                      break;
            case ']': t->state = ST_OSC_STR; t->osc_len = 0; break;
            case '7': t->saved_cx = t->cx; t->saved_cy = t->cy; break;
            case '8': t->cx = t->saved_cx; t->cy = t->saved_cy; clamp_cursor(t); break;
            case 'D': newline(t); break;
            case 'M': if (t->cy == t->scroll_top) scroll_down(t, 1);
                      else if (t->cy > 0) t->cy--;
                      break;
            case 'E': t->cx = 0; newline(t); break;
            case 'c': {  /* RIS: reset total */
                t->cur_fg = TSWL_COL_DEFAULT_FG;
                t->cur_bg = TSWL_COL_DEFAULT_BG;
                t->cur_attrs = 0;
                t->scroll_top = 0; t->scroll_bot = t->rows - 1;
                t->cx = 0; t->cy = 0;
                for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
                break;
            }
            case '=': case '>': case '(': case ')': case '#':
                /* modos de teclado/charset: ignorados conscientemente */
                if (b == '(' || b == ')' || b == '#') {
                    /* consome o próximo byte também (designador) — truque:
                     * volta pro estado ESC pra engolir 1 byte */
                    t->state = ST_ESC;
                }
                break;
            default: break;
            }
            continue;
        }

        if (t->state == ST_CSI) {
            /* R-11: ESC no meio de CSI cancela a sequência e inicia
             * um novo escape — comportamento de xterm/foot/vte. */
            if (b == 0x1B) {
                t->state = ST_ESC;
                t->csi_nparams = 0;
                t->csi_private = false;
                t->csi_intermed = 0;
                continue;
            }
            if (b >= '0' && b <= '9') {
                if (t->csi_nparams == 0) t->csi_nparams = 1;
                int *p = &t->csi_params[t->csi_nparams - 1];
                /* Evita overflow de int (UB). Para de acumular além
                 * de CSI_PARAM_MAX — terminais reais fazem o mesmo. */
                if (*p <= CSI_PARAM_MAX / 10) {
                    *p = *p * 10 + (b - '0');
                } else {
                    *p = CSI_PARAM_MAX;
                }
            } else if (b == ';') {
                if (t->csi_nparams < MAX_CSI_PARAMS) t->csi_nparams++;
            } else if (b == '?') {
                t->csi_private = true;
            } else if (b >= 0x20 && b <= 0x2F) {
                t->csi_intermed = (char)b;
            } else if (b >= 0x40 && b <= 0x7E) {  /* final byte */
                if (t->csi_nparams == 0 &&
                    (b == 'h' || b == 'l' || b == 'm' || b == 'r')) {
                    t->csi_nparams = 0;  /* SGR vazio = reset; h/l sem params = nada */
                } else if (t->csi_nparams == 0) {
                    t->csi_nparams = 1;  /* CSI A com params vazios = 1 param default */
                }
                csi_dispatch(t, (char)b);
                t->state = ST_GROUND;
            }
            /* intermediários 0x20-0x2F e outros: engolidos (sem efeito) */
            continue;
        }

        /* ST_GROUND */
        if (b == 0x1B) { t->state = ST_ESC; continue; }
        /* R-11: C1 (0x80-0x9F) — não imprimir U+FFFD. 0x9B = CSI 8-bit.
         * Só fora de sequência UTF-8: no meio dela, 80-9F são SEMPRE
         * bytes de continuação (nunca iniciam caractere) e vão pro
         * decodificador. */
        if (t->utf8_left == 0 && b >= 0x80 && b <= 0x9F) {
            if (b == 0x9B) {
                t->state = ST_CSI;
                t->csi_nparams = 0;
                t->csi_private = false;
                t->csi_intermed = 0;
                memset(t->csi_params, 0, sizeof(t->csi_params));
            }
            /* demais C1: ignorados (não viram glyph) */
            continue;
        }
        ground_byte(t, b);
    }
    return t->changed;
}

bool tswl_term_take_bell(tswl_term *t) {
    if (!t || !t->bell_pending) return false;
    t->bell_pending = false;
    return true;
}


static char *b64_decode(const char *in, size_t inlen, size_t *outlen)
{
    signed char tbl[256];
    for (int i = 0; i < 256; i++) tbl[i] = -1;
    for (int i = 0; i < 26; i++) {
        tbl['A' + i] = (signed char)i;
        tbl['a' + i] = (signed char)(26 + i);
    }
    for (int i = 0; i < 10; i++) tbl['0' + i] = (signed char)(52 + i);
    tbl['+'] = 62;
    tbl['/'] = 63;

    size_t max_out = (inlen / 4) * 3 + 3;
    char *out = malloc(max_out + 1);
    if (!out) return NULL;
    size_t o = 0;
    unsigned val = 0;
    int valb = -8;
    for (size_t i = 0; i < inlen; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=') break;
        if (c == '\n' || c == '\r') continue;
        signed char d = tbl[c];
        if (d < 0) { free(out); return NULL; }
        val = (val << 6) | (unsigned)d;
        valb += 6;
        if (valb >= 0) {
            out[o++] = (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    out[o] = 0;
    if (outlen) *outlen = o;
    return out;
}

static void osc_finish(tswl_term *t)
{
    if (t->osc_len > (int)sizeof(t->osc_buf) - 1)
        t->osc_len = (int)sizeof(t->osc_buf) - 1;
    if (t->osc_len < 0) t->osc_len = 0;
    t->osc_buf[t->osc_len] = 0;

    if (t->osc_len > 2 && (t->osc_buf[0] == '0' || t->osc_buf[0] == '2')
        && t->osc_buf[1] == ';') {
        snprintf(t->window_title, sizeof(t->window_title),
                 "%s", t->osc_buf + 2);
        t->title_pending = true;
        t->changed = true;
    } else if (t->osc_len > 4 && t->osc_buf[0] == '5' && t->osc_buf[1] == '2'
               && t->osc_buf[2] == ';') {
        const char *p = t->osc_buf + 3;
        while (*p && *p != ';') p++;
        if (*p == ';') {
            p++;
            if (*p && *p != '?') {
                size_t dlen = 0;
                char *decoded = b64_decode(p, strlen(p), &dlen);
                if (decoded) {
                    free(t->clip_pending);
                    t->clip_pending = decoded;
                    t->clip_pending_len = dlen;
                    t->clip_pending_set = true;
                    t->changed = true;
                }
            }
        }
    }
    t->osc_len = 0;
}

bool tswl_term_take_clipboard(tswl_term *t, char **out, size_t *outlen)
{
    if (!t || !t->clip_pending_set || !t->clip_pending) return false;
    if (out) *out = t->clip_pending;
    else free(t->clip_pending);
    if (outlen) *outlen = t->clip_pending_len;
    t->clip_pending = NULL;
    t->clip_pending_len = 0;
    t->clip_pending_set = false;
    return true;
}

bool tswl_term_take_title(tswl_term *t, char *out, size_t outsz) {
    if (!t || !out || outsz == 0) return false;
    if (!t->title_pending) return false;
    snprintf(out, outsz, "%s", t->window_title);
    t->title_pending = false;
    t->clip_pending = NULL;
    t->clip_pending_len = 0;
    t->clip_pending_set = false;
    return true;
}

void tswl_term_resize(tswl_term *t, int cols, int rows) {
    if (cols == t->cols && rows == t->rows) return;
    tswl_cell *new_grid = malloc((size_t)cols * rows * sizeof(tswl_cell));
    tswl_cell *new_main_save = malloc((size_t)cols * rows * sizeof(tswl_cell));
    uint8_t *new_dirty = malloc((size_t)rows);
    tswl_cell *new_back = malloc((size_t)TSWL_SCROLLBACK * cols * sizeof(tswl_cell));
    if (!new_grid || !new_main_save || !new_dirty || !new_back) {
        free(new_grid); free(new_main_save); free(new_dirty); free(new_back);
        return;  /* falha de memória: mantém o grid antigo (não crasha) */
    }
    tswl_cell b = blank_cell(t->cur_bg);
    for (int i = 0; i < cols * rows; i++) {
        new_grid[i] = b;
        new_main_save[i] = b;
    }

    /* Preserva o conteúdo ancorado no TOPO: quando a janela cresce, o
     * prompt continua no topo (espaço vazio embaixo, como xterm/foot).
     * Quando a janela encolhe, descarta só as linhas vazias do topo ou,
     * se o conteúdo encher mais que o novo tamanho, as mais antigas. */
    int used = 0;  /* última linha com célula não-vazia + 1 */
    for (int r = t->rows - 1; r >= 0; r--) {
        for (int x = 0; x < t->cols; x++) {
            if (t->grid[(size_t)r * t->cols + x].ch != 0) {
                used = r + 1;
                goto found_used;
            }
        }
    }
found_used:;
    int copy_rows = used < rows ? used : rows;
    int src_row = used > rows ? used - rows : 0;  /* descarta as linhas antigas excedentes */
    int dst_row = 0;                              /* conteúdo vai pro topo */
    int copy_cols = t->cols < cols ? t->cols : cols;
    for (int r = 0; r < copy_rows; r++) {
        memcpy(&new_grid[(size_t)(dst_row + r) * cols],
               &t->grid[(size_t)(src_row + r) * t->cols],
               (size_t)copy_cols * sizeof(tswl_cell));
        memcpy(&new_main_save[(size_t)(dst_row + r) * cols],
               &t->main_save[(size_t)(src_row + r) * t->cols],
               (size_t)copy_cols * sizeof(tswl_cell));
    }

    /* Migra scrollback para o novo número de colunas (antes zerava —
     * "scrollback não zerar no resize", AFAZERES fase TSWL). */
    tswl_cell bb = blank_cell(t->cur_bg);
    for (int i = 0; i < TSWL_SCROLLBACK * cols; i++)
        new_back[i] = bb;
    int new_count = t->back_count;
    if (new_count > TSWL_SCROLLBACK)
        new_count = TSWL_SCROLLBACK;
    int new_head = 0;
    int mig_cols = t->cols < cols ? t->cols : cols;
    for (int i = 0; i < new_count; i++) {
        int old_ring = (t->back_head - t->back_count + i + TSWL_SCROLLBACK)
                       % TSWL_SCROLLBACK;
        memcpy(&new_back[(size_t)new_head * cols],
               &t->back[(size_t)old_ring * t->cols],
               (size_t)mig_cols * sizeof(tswl_cell));
        new_head = (new_head + 1) % TSWL_SCROLLBACK;
    }
    if (new_count == 0)
        new_head = 0;

    free(t->grid); free(t->main_save); free(t->dirty); free(t->back);
    t->grid = new_grid;
    t->main_save = new_main_save;
    t->dirty = new_dirty;
    t->back = new_back;
    t->cols = cols;
    t->rows = rows;
    t->back_head = new_head;
    t->back_count = new_count;
    t->scroll_top = 0;
    t->scroll_bot = rows - 1;
    if (t->scroll_offset > t->back_count)
        t->scroll_offset = t->back_count;
    /* O cursor acompanha o conteúdo: a tela cresceu, o cursor fica onde
     * estava (conteúdo ficou no topo); a tela encolheu, o cursor sobe
     * junto (as primeiras linhas sumiram). Sem isso, o shell escreve na
     * linha errada depois do resize ("texto bugado"). */
    t->cy -= src_row;
    clamp_cursor(t);
    mark_all_dirty(t);
}

void tswl_term_scroll_view(tswl_term *t, int delta_lines) {
    t->scroll_offset += delta_lines;
    if (t->scroll_offset < 0) t->scroll_offset = 0;
    if (t->scroll_offset > t->back_count) t->scroll_offset = t->back_count;
    t->changed = true;
    mark_all_dirty(t);
}
