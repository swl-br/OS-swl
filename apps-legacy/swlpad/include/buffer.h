#ifndef SWLPAD_BUFFER_H
#define SWLPAD_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * buffer: buffer de texto editável do SWLPad, implementado como gap buffer.
 *
 * Gap buffer: o texto fica num array contíguo com um "buraco" (gap) na
 * posição do cursor. Inserir/apagar no cursor é O(1) (só move o gap),
 * sem realocar. É a estrutura clássica de editores (Emacs, etc.) — leve,
 * simples e eficiente pra texto.
 *
 * O texto é UTF-8 em bytes. O cursor é uma posição em bytes no texto
 * lógico (sem o gap). Movimentação por linha é feita navegando o texto.
 */

typedef struct swlpad_buffer swlpad_buffer;

/* criação/destruição */
swlpad_buffer *swlpad_buffer_new(void);
void swlpad_buffer_free(swlpad_buffer *b);

/* carrega texto de um arquivo pro buffer. Retorna false se o arquivo
 * não existe (buffer fica vazio — novo arquivo). */
bool swlpad_buffer_load(swlpad_buffer *b, const char *path);

/* salva o buffer num arquivo. Retorna false em erro de escrita. */
bool swlpad_buffer_save(swlpad_buffer *b, const char *path);

/* edição no cursor */
void swlpad_buffer_insert_char(swlpad_buffer *b, char byte);
void swlpad_buffer_insert_text(swlpad_buffer *b, const char *text, size_t len);
void swlpad_buffer_delete_back(swlpad_buffer *b);   /* backspace */
void swlpad_buffer_delete_forward(swlpad_buffer *b); /* delete */

/* movimentação do cursor (posição em bytes no texto lógico) */
size_t swlpad_buffer_cursor(const swlpad_buffer *b);
void swlpad_buffer_cursor_set(swlpad_buffer *b, size_t pos);
void swlpad_buffer_cursor_left(swlpad_buffer *b);
void swlpad_buffer_cursor_right(swlpad_buffer *b);
void swlpad_buffer_cursor_up(swlpad_buffer *b);
void swlpad_buffer_cursor_down(swlpad_buffer *b);
void swlpad_buffer_cursor_home(swlpad_buffer *b);   /* início da linha */
void swlpad_buffer_cursor_end(swlpad_buffer *b);    /* fim da linha */

/* acesso ao conteúdo (pra renderização e salvar) */
size_t swlpad_buffer_length(const swlpad_buffer *b);
/* copia o texto lógico pra out (sem o gap). out precisa ter length+1. */
void swlpad_buffer_get_text(const swlpad_buffer *b, char *out);
/* byte na posição lógica pos (0 = fora do range). */
char swlpad_buffer_byte_at(const swlpad_buffer *b, size_t pos);

/* linha/coluna do cursor (0-indexed), pra barra de status e navegação */
int swlpad_buffer_cursor_line(const swlpad_buffer *b);
int swlpad_buffer_cursor_col(const swlpad_buffer *b);

#endif /* SWLPAD_BUFFER_H */
