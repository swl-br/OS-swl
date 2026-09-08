/*   lexer.c -- turns SWL source text into a token stream.
 *
 * Line endings matter: a newline outside parentheses becomes a T_NL token
 * and separates statements.  Newlines inside parentheses are suppressed so
 * expressions may span lines.  `;` starts a comment until end of line and
 * slash-star ... star-slash are block comments that swallow newlines.
 * String literals support the escapes \\ \" \n \t \r \0 \'.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swlc.h"

const char *g_filename = "<input>";

static void die_msg(SrcPos p, const char *fmt, va_list ap)
{
    fprintf(stderr, "%s:%d:%d: error: ", g_filename, p.line, p.col);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    exit(1);
}

void die_at(SrcPos p, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    die_msg(p, fmt, ap);
    va_end(ap);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return q;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

char *xstrndup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

const char *tok_name(int kind)
{
    static const char *names[] = {
        "end of file", "end of line", "identifier", "number",
        "string literal",
        "module", "fn", "struct", "var", "if", "else", "while", "end",
        "return", "and", "or", "not",
        "const", "as", "break", "continue", "sizeof",
        "for", "to", "by", "global",
        "'('", "')'", "','", "':'", "'->'", "'.'", "'=', ';'",
        "+", "-", "*", "/", "%",
        "=='", "!=", "<'", ">'", "<=", ">=',",
        "'&'", "'['", "']'", "';'",
    };
    if (kind >= 0 && kind < (int)(sizeof(names) / sizeof(names[0])))
        return names[kind];
    return "?";
}

/* ---- keywords ---- */

struct kw { const char *word; int kind; };

static const struct kw keywords[] = {
    { "module", T_MODULE }, { "fn", T_FN }, { "struct", T_STRUCT },
    { "var", T_VAR }, { "if", T_IF }, { "else", T_ELSE }, { "while", T_WHILE },
    { "end", T_END }, { "return", T_RETURN }, { "and", T_AND },
    { "or", T_OR }, { "not", T_NOT },
    { "const", T_CONST }, { "as", T_AS },
    { "break", T_BREAK }, { "continue", T_CONTINUE },
    { "sizeof", T_SIZEOF },
    { "for", T_FOR }, { "to", T_TO }, { "by", T_BY },
    { "global", T_GLOBAL },
    { NULL, 0 }
};

static int keyword_kind(const char *s)
{
    int i;
    for (i = 0; keywords[i].word; i++)
        if (strcmp(s, keywords[i].word) == 0)
            return keywords[i].kind;
    return 0;
}

/* ---- character helpers ---- */

static int is_ident_start(int c)
{
    return isalpha(c) || c == '_';
}

static int is_ident_cont(int c)
{
    return isalnum(c) || c == '_';
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int is_hex(int c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

static void push_tok(Token **toks, int *n, int *cap, TokKind kind,
                     SrcPos pos, char *text, long long ival)
{
    Token *t;
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 64;
        *toks = xrealloc(*toks, sizeof(Token) * *cap);
    }
    t = &(*toks)[(*n)++];
    t->kind = kind;
    t->pos = pos;
    t->text = text;
    t->ival = ival;
}

Token *lex(const char *src, int *ntok)
{
    Token *toks = NULL;
    int n = 0, cap = 0;
    const char *s = src;
    int line = 1, col = 1;
    int paren = 0;   /* () nesting: NL suppressed while > 0 */

    SrcPos pos;
    char ch;

    for (;;) {
        ch = *s;

        /* skip spaces */
        while (ch == ' ' || ch == '\t' || ch == '\r') {
            s++;
            col++;
            ch = *s;
        }

        /* newline (outside parentheses) -> T_NL (runs collapse) */
        if (ch == '\n') {
            if (paren == 0)
                push_tok(&toks, &n, &cap, T_NL,
                         (SrcPos){ line, col }, NULL, 0);
            s++;
            line++;
            col = 1;
            continue;
        }

        if (ch == '\0') {
            push_tok(&toks, &n, &cap, T_EOF,
                     (SrcPos){ line, col }, NULL, 0);
            break;
        }

        pos.line = line;
        pos.col = col;

        /* comments */
        if (ch == ';') {
            while (*s && *s != '\n')
                s++, col++;
            continue;
        }
        if (ch == '/' && s[1] == '*') {
            int start_line = line, start_col = col;
            s += 2;
            col += 2;
            while (*s && !(*s == '*' && s[1] == '/')) {
                if (*s == '\n') { line++; col = 1; }
                else col++;
                s++;
            }
            if (!*s)
                die_at((SrcPos){ start_line, start_col },
                       "unterminated block comment");
            s += 2;
            col += 2;
            continue;
        }

        /* string literal */
        if (ch == '"') {
            int start_line = line, start_col = col;
            char *buf = NULL;
            int blen = 0, bcap = 0;
            s++;
            col++;
            for (;;) {
                if (!*s || *s == '\n')
                    die_at((SrcPos){ start_line, start_col },
                           "unterminated string literal");
                if (*s == '"') {
                    s++;
                    col++;
                    break;
                }
                int c;
                if (*s == '\\') {
                    s++;
                    col++;
                    switch (*s) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '0': c = '\0'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    case '\'': c = '\''; break;
                    case '\0':
                        die_at((SrcPos){ start_line, start_col },
                               "unterminated string literal");
                        break;
                    default:
                        die_at((SrcPos){ line, col },
                               "unknown escape sequence '\\%c' in string",
                               *s);
                        break;
                    }
                    s++;
                    col++;
                } else {
                    c = (unsigned char)*s;
                    s++;
                    col++;
                }
                if (blen + 1 > bcap) {
                    bcap = bcap ? bcap * 2 : 16;
                    buf = xrealloc(buf, bcap);
                }
                buf[blen++] = (char)c;
            }
            if (blen + 1 > bcap)
                buf = xrealloc(buf, bcap + 1);
            buf[blen] = '\0';
            push_tok(&toks, &n, &cap, T_STR, pos, buf, blen);
            continue;
        }

        /* identifiers / keywords */
        if (is_ident_start(ch)) {
            const char *begin = s;
            int len = 0;
            while (is_ident_cont(*s)) {
                s++;
                len++;
                col++;
            }
            char *text = xstrndup(begin, len);
            int k = keyword_kind(text);
            push_tok(&toks, &n, &cap, k ? k : T_IDENT, pos, text, 0);
            continue;
        }

        /* numbers: decimal or 0x hex */
        if (is_digit(ch)) {
            long long v = 0;
            if (ch == '0' && (s[1] == 'x' || s[1] == 'X')) {
                s += 2;
                col += 2;
                if (!is_hex(*s))
                    die_at(pos, "expected hexadecimal digits after '0x'");
                while (is_hex(*s)) {
                    v = v * 16 + hex_val(*s);
                    if (v > 0x7FFFFFFFFFFFFFFFLL)
                        die_at(pos, "integer literal too large");
                    s++;
                    col++;
                }
            } else {
                while (is_digit(*s)) {
                    v = v * 10 + (*s - '0');
                    if (v > 0x7FFFFFFFFFFFFFFFLL)
                        die_at(pos, "integer literal too large");
                    s++;
                    col++;
                }
                if (is_ident_start(*s) || is_ident_cont(*s))
                    die_at(pos, "invalid number");
            }
            push_tok(&toks, &n, &cap, T_INT, pos, NULL, v);
            continue;
        }

        /* punctuation and operators */
        {
            int kind = T_EOF;
            int nxt = s[1];

            switch (ch) {
            case '(': kind = T_LPAREN; paren++; break;
            case ')': kind = T_RPAREN; if (paren > 0) paren--; break;
            case ',': kind = T_COMMA; break;
            case ':': kind = T_COLON; break;
            case '[': kind = T_LBRACK; break;
            case ']': kind = T_RBRACK; break;
            case '.': kind = T_DOT; break;
            case '+': kind = T_PLUS; break;
            case '*': kind = T_STAR; break;
            case '/': kind = T_SLASH; break;
            case '%': kind = T_PERCENT; break;
            case '&': kind = T_AMP; break;
            case ';': kind = T_SEMICOLON; break;
            case '-':
                if (nxt == '>') { kind = T_ARROW; s++; col++; }
                else kind = T_MINUS;
                break;
            case '=':
                if (nxt == '=') { kind = T_EQ; s++; col++; }
                else kind = T_ASSIGN;
                break;
            case '!':
                if (nxt == '=') { kind = T_NE; s++; col++; }
                else die_at(pos, "unexpected character '!' (did you mean '!=?')");
                break;
            case '<':
                if (nxt == '=') { kind = T_LE; s++; col++; }
                else kind = T_LT;
                break;
            case '>':
                if (nxt == '=') { kind = T_GE; s++; col++; }
                else kind = T_GT;
                break;
            default:
                die_at(pos, "unexpected character '%c' (0x%02x)", ch,
                       (unsigned char)ch);
            }

            push_tok(&toks, &n, &cap, kind, pos, NULL, 0);
            s++;
            col++;
        }
    }

    *ntok = n;
    return toks;
}
