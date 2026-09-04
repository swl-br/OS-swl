/*
 * term.c — grid de células + parser ANSI/VT100 do TSWL.
 *
 * Parser: máquina de estados byte a byte (GROUND → ESC → CSI → OSC).
 * Decodificação UTF-8 incremental embutida (1-4 bytes → codepoint).
 * Nenhuma alocação acontece em tswl_term_feed — toda a memória é
 * pré-alocada na criação/resize.
 */

#include <stdlib.h>
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
    tswl_cell *back;          /* ring buffer: TSWL_SCROLLBACK * cols */
    int back_head;            /* próxima posição de escrita no ring */
    int back_count;           /* linhas válidas no ring (<= TSWL_SCROLLBACK) */
    uint8_t *dirty;           /* 1 byte por linha da tela */

    int cx, cy;               /* cursor */
    int saved_cx, saved_cy;
    uint16_t cur_fg, cur_bg;
    uint8_t cur_attrs;
    bool cursor_visible;
    int scroll_top, scroll_bot;  /* região de scroll (linhas, inclusivo) */

    int scroll_offset;        /* scrollback visual (0 = fim) */

    enum parser_state state;
    int csi_params[MAX_CSI_PARAMS];
    int csi_nparams;
    bool csi_private;         /* '?' logo após '[' */
    uint32_t utf8_cp;         /* codepoint parcial */
    int utf8_left;            /* bytes restantes do caractere atual */
    bool changed;
};

static tswl_cell blank_cell(uint16_t bg) {
    tswl_cell c = { 0, TSWL_COL_DEFAULT_FG, bg, 0 };
    return c;
}

tswl_term *tswl_term_new(int cols, int rows) {
    tswl_term *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->grid = malloc((size_t)cols * rows * sizeof(tswl_cell));
    t->back = malloc((size_t)TSWL_SCROLLBACK * cols * sizeof(tswl_cell));
    t->dirty = malloc((size_t)rows);
    if (!t->grid || !t->back || !t->dirty) {
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
    for (int i = 0; i < cols * rows; i++) t->grid[i] = b;
    memset(t->dirty, 1, (size_t)rows);
    return t;
}

void tswl_term_free(tswl_term *t) {
    if (!t) return;
    free(t->grid);
    free(t->back);
    free(t->dirty);
    free(t);
}

int tswl_term_cols(const tswl_term *t) { return t->cols; }
int tswl_term_rows(const tswl_term *t) { return t->rows; }
int tswl_term_cursor_x(const tswl_term *t) { return t->cx; }
int tswl_term_cursor_y(const tswl_term *t) { return t->cy; }
bool tswl_term_cursor_visible(const tswl_term *t) { return t->cursor_visible; }
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

void tswl_term_clear_dirty(tswl_term *t) {
    memset(t->dirty, 0, (size_t)t->rows);
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
    for (int k = 0; k < n; k++) {
        if (t->scroll_top == 0 && t->scroll_bot == t->rows - 1) {
            memcpy(&t->back[(size_t)t->back_head * t->cols],
                   &t->grid[0], (size_t)t->cols * sizeof(tswl_cell));
            t->back_head = (t->back_head + 1) % TSWL_SCROLLBACK;
            if (t->back_count < TSWL_SCROLLBACK) t->back_count++;
        }
        memmove(&t->grid[(size_t)t->scroll_top * t->cols],
                &t->grid[(size_t)(t->scroll_top + 1) * t->cols],
                (size_t)(t->scroll_bot - t->scroll_top) * t->cols * sizeof(tswl_cell));
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
    for (int k = 0; k < n; k++) {
        memmove(&t->grid[(size_t)(t->scroll_top + 1) * t->cols],
                &t->grid[(size_t)t->scroll_top * t->cols],
                (size_t)(t->scroll_bot - t->scroll_top) * t->cols * sizeof(tswl_cell));
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

static int param(tswl_term *t, int i, int def) {
    if (i >= t->csi_nparams || t->csi_params[i] == 0) return def;
    return t->csi_params[i];
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
        else if (p == 4) t->cur_attrs |= TSWL_ATTR_UNDERLINE;
        else if (p == 7) t->cur_attrs |= TSWL_ATTR_REVERSE;
        else if (p == 22) t->cur_attrs &= ~(TSWL_ATTR_BOLD | TSWL_ATTR_DIM);
        else if (p == 24) t->cur_attrs &= ~TSWL_ATTR_UNDERLINE;
        else if (p == 27) t->cur_attrs &= ~TSWL_ATTR_REVERSE;
        else if (p >= 30 && p <= 37) t->cur_fg = (uint16_t)(p - 30);
        else if (p == 39) t->cur_fg = TSWL_COL_DEFAULT_FG;
        else if (p >= 40 && p <= 47) t->cur_bg = (uint16_t)(p - 40);
        else if (p == 49) t->cur_bg = TSWL_COL_DEFAULT_BG;
        else if (p >= 90 && p <= 97) t->cur_fg = (uint16_t)(p - 90 + 8);
        else if (p >= 100 && p <= 107) t->cur_bg = (uint16_t)(p - 100 + 8);
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
        } else if (n == 2 || n == 3) {
            for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
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
    case 'S': scroll_up(t, param(t, 0, 1)); break;
    case 'T': scroll_down(t, param(t, 0, 1)); break;
    case 'X': {  /* erase chars */
        int cnt = param(t, 0, 1);
        erase_range(t, t->cy, t->cx, t->cx + cnt - 1);
        break;
    }
    case 'd': t->cy = param(t, 0, 1) - 1; clamp_cursor(t); break;
    case 'm': csi_sgr(t); break;
    case 'r':  /* set scroll region */
        t->scroll_top = param(t, 0, 1) - 1;
        t->scroll_bot = param(t, 1, t->rows) - 1;
        if (t->scroll_top < 0) t->scroll_top = 0;
        if (t->scroll_bot >= t->rows) t->scroll_bot = t->rows - 1;
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
            } else if (t->csi_private && t->csi_params[i] == 1049) {
                /* alt screen: primeira versão = limpa a tela e vai,
                 * sai restaurando cursor. Suficiente pro htop/top não
                 * sujarem o histórico. */
                if (set) {
                    t->saved_cx = t->cx; t->saved_cy = t->cy;
                    for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
                    t->cx = 0; t->cy = 0;
                } else {
                    for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols - 1);
                    t->cx = t->saved_cx; t->cy = t->saved_cy;
                    clamp_cursor(t);
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
            /* OSC ... até BEL ou ESC \ — conteúdo ignorado (título etc.) */
            if (t->state == ST_OSC_ESC && b == '\\') {
                t->state = ST_GROUND;
            } else if (b == 0x1B) {
                t->state = ST_OSC_ESC;
            } else if (b == '\a') {
                t->state = ST_GROUND;
            } else if (t->state == ST_OSC_ESC) {
                t->state = ST_OSC_STR;  /* ESC solto dentro de OSC */
            }
            continue;
        }

        if (t->state == ST_ESC) {
            t->state = ST_GROUND;
            switch (b) {
            case '[': t->state = ST_CSI; t->csi_nparams = 0;
                      t->csi_private = false;
                      memset(t->csi_params, 0, sizeof(t->csi_params));
                      break;
            case ']': t->state = ST_OSC_STR; break;
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
            if (b >= '0' && b <= '9') {
                if (t->csi_nparams == 0) t->csi_nparams = 1;
                t->csi_params[t->csi_nparams - 1] =
                    t->csi_params[t->csi_nparams - 1] * 10 + (b - '0');
            } else if (b == ';') {
                if (t->csi_nparams < MAX_CSI_PARAMS) t->csi_nparams++;
            } else if (b == '?') {
                t->csi_private = true;
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
            continue;
        }

        /* ST_GROUND */
        if (b == 0x1B) { t->state = ST_ESC; continue; }
        ground_byte(t, b);
    }
    return t->changed;
}

void tswl_term_resize(tswl_term *t, int cols, int rows) {
    if (cols == t->cols && rows == t->rows) return;
    tswl_cell *new_grid = malloc((size_t)cols * rows * sizeof(tswl_cell));
    uint8_t *new_dirty = malloc((size_t)rows);
    tswl_cell *new_back = malloc((size_t)TSWL_SCROLLBACK * cols * sizeof(tswl_cell));
    if (!new_grid || !new_dirty || !new_back) {
        free(new_grid); free(new_dirty); free(new_back);
        return;  /* falha de memória: mantém o grid antigo (não crasha) */
    }
    tswl_cell b = blank_cell(t->cur_bg);
    for (int i = 0; i < cols * rows; i++) new_grid[i] = b;

    /* Preserva o conteúdo ancorado embaixo: as últimas min(rows) linhas
     * visíveis do grid antigo vão pras últimas linhas do grid novo. Assim
     * o prompt (que normalmente está embaixo) continua no mesmo lugar
     * visual. delta_y é quanto cada linha de conteúdo andou na tela. */
    int copy_rows = t->rows < rows ? t->rows : rows;
    int delta_y = rows - t->rows;
    int src_row = t->rows - copy_rows;
    int dst_row = rows - copy_rows;
    int copy_cols = t->cols < cols ? t->cols : cols;
    for (int r = 0; r < copy_rows; r++) {
        memcpy(&new_grid[(size_t)(dst_row + r) * cols],
               &t->grid[(size_t)(src_row + r) * t->cols],
               (size_t)copy_cols * sizeof(tswl_cell));
    }

    free(t->grid); free(t->dirty); free(t->back);
    t->grid = new_grid;
    t->dirty = new_dirty;
    t->back = new_back;
    t->cols = cols;
    t->rows = rows;
    t->back_head = 0;
    t->back_count = 0;  /* histórico não migra entre resizes (simples e seguro) */
    t->scroll_top = 0;
    t->scroll_bot = rows - 1;
    t->scroll_offset = 0;
    /* O cursor acompanha o conteúdo: se a tela cresceu, ele desce junto
     * (o conteúdo desceu); se encolheu, ele sobe. Sem isso, o shell
     * escreve na linha errada depois do resize ("texto bugado"). */
    t->cy += delta_y;
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
