/*
 * buffer.c — gap buffer do SWLPad.
 *
 * Estrutura: um array `data` de capacidade `cap` bytes, com um gap entre
 * `gap_start` e `gap_end`. O texto lógico é:
 *   data[0 .. gap_start) + data[gap_end .. cap)
 * O cursor (posição lógica) é sempre gap_start.
 *
 * Inserir: escreve em data[gap_start++] (gap encolhe pela esquerda).
 * Apagar antes do cursor: gap_start-- (gap cresce pra esquerda).
 * Apagar depois do cursor: gap_end++ (gap cresce pra direita).
 * Mover o cursor: move o gap com memmove.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"

#define GAP_INITIAL_CAP 4096

struct swlpad_buffer {
    char *data;        /* array com o gap */
    size_t cap;        /* capacidade total do array */
    size_t gap_start;  /* início do gap = posição do cursor */
    size_t gap_end;    /* fim do gap (exclusivo) */
};

/* posição física no array `data` correspondente à posição lógica `pos` */
static size_t phys(const swlpad_buffer *b, size_t pos) {
    return pos < b->gap_start ? pos : pos + (b->gap_end - b->gap_start);
}

static size_t gap_size(const swlpad_buffer *b) {
    return b->gap_end - b->gap_start;
}

static size_t text_len(const swlpad_buffer *b) {
    return b->cap - gap_size(b);
}

swlpad_buffer *swlpad_buffer_new(void) {
    swlpad_buffer *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->data = malloc(GAP_INITIAL_CAP);
    if (!b->data) {
        free(b);
        return NULL;
    }
    b->cap = GAP_INITIAL_CAP;
    b->gap_start = 0;
    b->gap_end = b->cap;  /* gap = tudo (buffer vazio) */
    return b;
}

void swlpad_buffer_free(swlpad_buffer *b) {
    if (!b) return;
    free(b->data);
    free(b);
}

size_t swlpad_buffer_length(const swlpad_buffer *b) {
    return text_len(b);
}

size_t swlpad_buffer_cursor(const swlpad_buffer *b) {
    return b->gap_start;
}

char swlpad_buffer_byte_at(const swlpad_buffer *b, size_t pos) {
    if (pos >= text_len(b)) return 0;
    return b->data[phys(b, pos)];
}

void swlpad_buffer_get_text(const swlpad_buffer *b, char *out) {
    size_t before = b->gap_start;
    size_t after = b->cap - b->gap_end;
    memcpy(out, b->data, before);
    memcpy(out + before, b->data + b->gap_end, after);
    out[before + after] = 0;
}

/* garante que o gap tem pelo menos `need` bytes livres */
static bool ensure_gap(swlpad_buffer *b, size_t need) {
    if (gap_size(b) >= need) return true;
    size_t old_text = text_len(b);
    size_t new_cap = b->cap * 2;
    while (new_cap - old_text < need) new_cap *= 2;
    char *nd = malloc(new_cap);
    if (!nd) return false;
    /* texto antes do gap fica no início */
    memcpy(nd, b->data, b->gap_start);
    /* texto depois do gap fica no fim */
    size_t after = b->cap - b->gap_end;
    memcpy(nd + new_cap - after, b->data + b->gap_end, after);
    free(b->data);
    b->data = nd;
    b->cap = new_cap;
    b->gap_end = new_cap - after;
    return true;
}

/* move o gap pra posição lógica `pos` */
static void move_gap(swlpad_buffer *b, size_t pos) {
    size_t len = text_len(b);
    if (pos > len) pos = len;
    if (pos == b->gap_start) return;
    if (pos < b->gap_start) {
        /* move texto da esquerda do gap pra direita do gap */
        size_t n = b->gap_start - pos;
        memmove(b->data + b->gap_end - n, b->data + pos, n);
        b->gap_start = pos;
        b->gap_end -= n;
    } else {
        /* move texto da direita do gap pra esquerda do gap */
        size_t n = pos - b->gap_start;
        memmove(b->data + b->gap_start, b->data + b->gap_end, n);
        b->gap_start = pos;
        b->gap_end += n;
    }
}

void swlpad_buffer_cursor_set(swlpad_buffer *b, size_t pos) {
    move_gap(b, pos);
}

void swlpad_buffer_insert_text(swlpad_buffer *b, const char *text, size_t len) {
    if (len == 0) return;
    if (!ensure_gap(b, len)) return;
    memcpy(b->data + b->gap_start, text, len);
    b->gap_start += len;
}

void swlpad_buffer_insert_char(swlpad_buffer *b, char byte) {
    swlpad_buffer_insert_text(b, &byte, 1);
}

void swlpad_buffer_delete_back(swlpad_buffer *b) {
    if (b->gap_start == 0) return;
    b->gap_start--;
}

void swlpad_buffer_delete_forward(swlpad_buffer *b) {
    if (b->gap_end >= b->cap) return;
    b->gap_end++;
}

/* encontra o início da linha que contém pos (posição lógica) */
static size_t line_start(const swlpad_buffer *b, size_t pos) {
    while (pos > 0 && swlpad_buffer_byte_at(b, pos - 1) != '\n') pos--;
    return pos;
}

/* encontra o fim da linha que contém pos (posição do '\n' ou fim) */
static size_t line_end(const swlpad_buffer *b, size_t pos) {
    size_t len = text_len(b);
    while (pos < len && swlpad_buffer_byte_at(b, pos) != '\n') pos++;
    return pos;
}

void swlpad_buffer_cursor_left(swlpad_buffer *b) {
    if (b->gap_start > 0) move_gap(b, b->gap_start - 1);
}

void swlpad_buffer_cursor_right(swlpad_buffer *b) {
    if (b->gap_start < text_len(b)) move_gap(b, b->gap_start + 1);
}

void swlpad_buffer_cursor_home(swlpad_buffer *b) {
    move_gap(b, line_start(b, b->gap_start));
}

void swlpad_buffer_cursor_end(swlpad_buffer *b) {
    move_gap(b, line_end(b, b->gap_start));
}

int swlpad_buffer_cursor_line(const swlpad_buffer *b) {
    int line = 0;
    size_t pos = b->gap_start;
    for (size_t i = 0; i < pos; i++) {
        if (swlpad_buffer_byte_at(b, i) == '\n') line++;
    }
    return line;
}

int swlpad_buffer_cursor_col(const swlpad_buffer *b) {
    return (int)(b->gap_start - line_start(b, b->gap_start));
}

void swlpad_buffer_cursor_up(swlpad_buffer *b) {
    size_t start = line_start(b, b->gap_start);
    if (start == 0) return;  /* já na primeira linha */
    int col = swlpad_buffer_cursor_col(b);
    size_t prev_end = start - 1;              /* o '\n' da linha anterior */
    size_t prev_start = line_start(b, prev_end);
    size_t prev_len = prev_end - prev_start;
    size_t target = prev_start + ((size_t)col < prev_len ? (size_t)col : prev_len);
    move_gap(b, target);
}

void swlpad_buffer_cursor_down(swlpad_buffer *b) {
    size_t len = text_len(b);
    size_t end = line_end(b, b->gap_start);
    if (end >= len) return;  /* já na última linha */
    int col = swlpad_buffer_cursor_col(b);
    size_t next_start = end + 1;              /* depois do '\n' */
    size_t next_end = line_end(b, next_start);
    size_t next_len = next_end - next_start;
    size_t target = next_start + ((size_t)col < next_len ? (size_t)col : next_len);
    move_gap(b, target);
}

bool swlpad_buffer_load(swlpad_buffer *b, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return false; }
    if (!ensure_gap(b, (size_t)size)) { fclose(f); return false; }
    size_t n = fread(b->data + b->gap_start, 1, (size_t)size, f);
    b->gap_start += n;
    fclose(f);
    return true;
}

bool swlpad_buffer_save(swlpad_buffer *b, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    /* escreve as duas metades do texto (antes e depois do gap) */
    size_t before = b->gap_start;
    size_t after = b->cap - b->gap_end;
    bool ok = fwrite(b->data, 1, before, f) == before &&
              fwrite(b->data + b->gap_end, 1, after, f) == after;
    fclose(f);
    return ok;
}
