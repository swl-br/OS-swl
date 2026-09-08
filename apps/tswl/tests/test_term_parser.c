/*
 * test_term_parser.c — unit tests do parser ANSI do TSWL (sem Wayland).
 *
 * Cobre regressões R-01/R-03/R-11/R-14 e casos básicos de cursor.
 * Compila só com term.c:
 *   cc -std=c11 -O0 -g -fsanitize=address,undefined \
 *      -I../include -o test_term_parser test_term_parser.c ../src/term.c
 *   ./test_term_parser
 */
#include "term.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;
static int passes;

static void expect(int cond, const char *msg)
{
    if (cond) {
        printf("ok   %s\n", msg);
        passes++;
    } else {
        printf("FAIL %s\n", msg);
        fails++;
    }
}

static void feed(tswl_term *t, const char *s)
{
    tswl_term_feed(t, s, strlen(s));
}

/* R-01 / R-03: região de scroll inválida não corrompe memória */
static void test_csi_r_invalid(void)
{
    tswl_term *t = tswl_term_new(80, 24);
    feed(t, "\033[9999;1r");
    feed(t, "\033[5S"); /* scroll up — antes: memmove size_t gigante */
    expect(tswl_term_cursor_x(t) == 0 && tswl_term_cursor_y(t) == 0,
           "R-01 CSI r inválido + S não crasha e cursor em home");
    tswl_term_free(t);
}

static void test_csi_repeat_clamp(void)
{
    tswl_term *t = tswl_term_new(80, 24);
    /* muitos dígitos no param — não overflow */
    feed(t, "\033[999999999999999999999A");
    expect(tswl_term_cursor_y(t) >= 0 && tswl_term_cursor_y(t) < 24,
           "R-03 param gigante clampado (cursor em range)");
    tswl_term_free(t);
}

/* R-11: ESC no meio de CSI cancela */
static void test_esc_cancels_csi(void)
{
    tswl_term *t = tswl_term_new(80, 24);
    /* posiciona em (5,5) */
    feed(t, "\033[6;6H");
    expect(tswl_term_cursor_x(t) == 5 && tswl_term_cursor_y(t) == 5,
           "setup cursor 5,5");
    /* CSI incompleto + ESC + cursor up 1 */
    feed(t, "\033[1;2\033[A");
    expect(tswl_term_cursor_y(t) == 4,
           "R-11 ESC cancela CSI; [A sobe 1 linha");
    tswl_term_free(t);
}

/* R-11: C1 não vira glyph; 0x9B inicia CSI */
static void test_c1_and_csi_8bit(void)
{
    tswl_term *t = tswl_term_new(80, 24);
    feed(t, "\033[10;10H");
    char seq[] = { (char)0x80, (char)0x9F, 0 };
    feed(t, seq);
    /* cursor não deve ter andado por glyphs */
    expect(tswl_term_cursor_x(t) == 9 && tswl_term_cursor_y(t) == 9,
           "R-11 C1 0x80-0x9F não imprimem");
    /* 0x9B = CSI, depois A = up */
    char csi8[] = { (char)0x9B, 'A', 0 };
    feed(t, csi8);
    expect(tswl_term_cursor_y(t) == 8, "R-11 0x9B inicia CSI (cursor up)");
    tswl_term_free(t);
}

/* R-14 DECCKM */
static void test_decckm(void)
{
    tswl_term *t = tswl_term_new(80, 24);
    expect(!tswl_term_app_cursor(t), "DECCKM off por padrão");
    feed(t, "\033[?1h");
    expect(tswl_term_app_cursor(t), "R-14 CSI ?1h liga app_cursor");
    feed(t, "\033[?1l");
    expect(!tswl_term_app_cursor(t), "R-14 CSI ?1l desliga app_cursor");
    tswl_term_free(t);
}

static void test_basic_text_and_crlf(void)
{
    tswl_term *t = tswl_term_new(40, 10);
    feed(t, "hi\r\n");
    const tswl_cell *c0 = tswl_term_cell(t, 0, 0);
    const tswl_cell *c1 = tswl_term_cell(t, 1, 0);
    expect(c0->ch == 'h' && c1->ch == 'i', "texto 'hi' na linha 0");
    expect(tswl_term_cursor_x(t) == 0 && tswl_term_cursor_y(t) == 1,
           "CRLF move cursor para início da próxima linha");
    tswl_term_free(t);
}

static void test_sgr_bold(void)
{
    tswl_term *t = tswl_term_new(40, 10);
    feed(t, "\033[1mX");
    const tswl_cell *c = tswl_term_cell(t, 0, 0);
    expect(c->ch == 'X' && (c->attrs & TSWL_ATTR_BOLD),
           "SGR 1 negrito na célula");
    tswl_term_free(t);
}

static void test_cup_clamp(void)
{
    tswl_term *t = tswl_term_new(10, 5);
    feed(t, "\033[99;99H");
    expect(tswl_term_cursor_x(t) < 10 && tswl_term_cursor_y(t) < 5,
           "CUP fora dos limites é clampado");
    tswl_term_free(t);
}


/* Scrollback migra no resize (não zera) */
static void test_scrollback_survives_resize(void)
{
    tswl_term *t = tswl_term_new(20, 5);
    /* enche a tela e força scroll → linhas no ring */
    for (int i = 0; i < 12; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "L%02d\n", i);
        feed(t, buf);
    }
    int before = 0;
    /* força offset via API se back_count > 0 — não temos getter de count,
     * então só confere que após resize o feed não crasha e cursor ok.
     * Marca uma linha conhecida: após vários \\n o histórico tem "L00". */
    tswl_term_resize(t, 30, 8);
    expect(tswl_term_cols(t) == 30 && tswl_term_rows(t) == 8,
           "resize 30x8 aplica dims");
    /* scroll_view pro máximo: se histórico migrou, offset > 0 é possível */
    tswl_term_scroll_view(t, 1000);
    int off = tswl_term_scroll_offset(t);
    expect(off > 0, "scrollback sobreviveu ao resize (offset > 0)");
    tswl_term_free(t);
    (void)before;
}


/* Alt screen 1049: preserva a tela principal */
static void test_alt_screen_restores(void)
{
    tswl_term *t = tswl_term_new(20, 6);
    feed(t, "HELLO");
    expect(tswl_term_cell(t, 0, 0)->ch == 'H', "setup HELLO");
    feed(t, "\033[?1049h");
    expect(tswl_term_cell(t, 0, 0)->ch == 0, "alt screen limpa a tela");
    feed(t, "ALT");
    expect(tswl_term_cell(t, 0, 0)->ch == 'A', "escreve no alt");
    feed(t, "\033[?1049l");
    expect(tswl_term_cell(t, 0, 0)->ch == 'H' &&
           tswl_term_cell(t, 1, 0)->ch == 'E',
           "sai do alt: restaura HELLO");
    tswl_term_free(t);
}

int main(void)
{
    printf("tswl term parser unit tests\n");
    test_basic_text_and_crlf();
    test_sgr_bold();
    test_cup_clamp();
    test_csi_r_invalid();
    test_csi_repeat_clamp();
    test_esc_cancels_csi();
    test_c1_and_csi_8bit();
    test_decckm();
    test_scrollback_survives_resize();
    test_alt_screen_restores();
    printf("\nsummary: %d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
