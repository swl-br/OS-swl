#ifndef TSWL_TERM_H
#define TSWL_TERM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * term: modelo de terminal do TSWL — grid de células + parser ANSI/VT100.
 *
 * O modelo é deliberadamente simples e barato:
 * - grid contíguo cols*rows de tswl_cell (atributos compactados em bits);
 * - scrollback em ring buffer de linhas (TSWL_SCROLLBACK linhas);
 * - parser como máquina de estados byte a byte, sem alocação por byte;
 * - renderer consulta o grid e desenha só as linhas sujas.
 *
 * Não é um emulador completo de xterm — cobre o subconjunto usado por
 * shells e apps de texto comuns: texto, cores 8/8-bright, negrito,
 * movimentação de cursor, erase, scroll, CRLF, tab, backspace, save/
 * restore, e os modos essenciais (cursor visível, alt screen ignorada
 * por enquanto virar necessidade real).
 */

/* cores padrão (índices na paleta do render) */
enum {
    TSWL_COL_BLACK = 0, TSWL_COL_RED, TSWL_COL_GREEN, TSWL_COL_YELLOW,
    TSWL_COL_BLUE, TSWL_COL_MAGENTA, TSWL_COL_CYAN, TSWL_COL_WHITE,
    TSWL_COL_DEFAULT_FG = 256, TSWL_COL_DEFAULT_BG = 257,
};

#define TSWL_ATTR_BOLD      0x01
#define TSWL_ATTR_DIM       0x02
#define TSWL_ATTR_UNDERLINE 0x04
#define TSWL_ATTR_REVERSE   0x08

typedef struct {
    uint32_t ch;        /* codepoint UTF-32 (0 = célula vazia) */
    uint16_t fg;        /* 0-7 normal, 8-15 bright, 256 = default */
    uint16_t bg;
    uint8_t  attrs;
} tswl_cell;

#define TSWL_SCROLLBACK 1000  /* linhas de histórico além da tela */

typedef struct tswl_term tswl_term;

/* criação/destruição — cols/rows iniciais; redimensionar depois com
 * tswl_term_resize (chamado quando a janela muda de tamanho). */
tswl_term *tswl_term_new(int cols, int rows);
void tswl_term_free(tswl_term *t);

/* alimenta o parser com bytes vindos do PTY. Retorna true se a tela
 * mudou (renderer deve redesenhar). */
bool tswl_term_feed(tswl_term *t, const char *data, size_t len);

/* redimensiona o grid preservando conteúdo visível o máximo possível. */
void tswl_term_resize(tswl_term *t, int cols, int rows);

/* acesso ao grid para o renderer */
int tswl_term_cols(const tswl_term *t);
int tswl_term_rows(const tswl_term *t);
const tswl_cell *tswl_term_cell(const tswl_term *t, int col, int row);
int tswl_term_cursor_x(const tswl_term *t);
int tswl_term_cursor_y(const tswl_term *t);
bool tswl_term_cursor_visible(const tswl_term *t);

/* scrollback: quantas linhas para trás a tela está deslocada
 * (0 = posição normal, no fim do histórico). O renderer usa
 * tswl_term_scrollback_line(t, row) em vez de tswl_term_cell quando
 * o usuário deu scroll pra cima. */
int tswl_term_scroll_offset(const tswl_term *t);
void tswl_term_scroll_view(tswl_term *t, int delta_lines);
const tswl_cell *tswl_term_scrollback_cell(const tswl_term *t, int col, int row);

/* sujeira por linha (render parcial): marca tudo sujo após resize. */
bool tswl_term_row_dirty(tswl_term *t, int row);
void tswl_term_clear_dirty(tswl_term *t);

#endif /* TSWL_TERM_H */
