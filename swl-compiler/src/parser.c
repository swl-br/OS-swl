/*
 * parser.c -- recursive-descent parser for the SWL MVP/M2 subset.
 *
 * Grammar (statements are one per line; blocks end with `end`):
 *
 *   program    := 'module' IDENT NL toplevel*
 *   toplevel   := structdecl | constdecl | fndecl
 *   structdecl := 'struct' IDENT NL field+ 'end'
 *   field      := IDENT ':' type NL
 *   constdecl  := 'const' IDENT ':' type '=' INTLIT NL
 *   fndecl     := 'fn' IDENT '(' params? ')' ('->' type)? NL stmt* 'end'
 *   params     := param (',' param)*
 *   param      := IDENT ':' type
 *   type       := '*'* (int type | struct name)     (pointers wrap a base)
 *   stmt       := 'var' IDENT ':' type ('=' expr)?
 *               | 'return' expr?
 *               | 'if' expr NL stmt* ('else' NL stmt*)? 'end'
 *               | 'while' expr NL stmt* 'end'
 *               | 'break' | 'continue'
 *               | IDENT ('.' IDENT)* '=' expr
 *               | '*'+ IDENT '=' expr               (write through pointer)
 *               | call '(' args ')'
 *   expr       := or ('or' or)* ...
 *   cast       := unary ('as' int type)*
 *   unary      := ('-'|'+'|'not'|'*'|'&') unary | primary(castable)
 *   primary    := INT | STR | IDENT call? | '(' expr ')'
 *
 * Primary identifiers may be followed by '.' field chains (variable reads).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swlc.h"

typedef struct {
    Token *t;
    int n;
    int i;
    Program *prog;
} Parser;

static Token *peek(Parser *p)
{
    return &p->t[p->i];
}

static Token *next(Parser *p)
{
    return &p->t[p->i++];
}

static int at(Parser *p, TokKind k)
{
    return peek(p)->kind == k;
}

static void expect(Parser *p, TokKind k, const char *fmt, ...)
{
    if (!at(p, k)) {
        Token *t = peek(p);
        char msg[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        die_at(t->pos, "expected %s, found %s%s%s%s",
               msg, tok_name(t->kind),
               t->kind == T_IDENT || t->kind == T_STR ? "'" : "",
               t->kind == T_IDENT || t->kind == T_STR ? t->text : "",
               t->kind == T_IDENT || t->kind == T_STR ? "'" : "");
    }
    next(p);
}

static void expect_nl(Parser *p)
{
    if (at(p, T_NL))
        next(p);
}

static Fn *add_fn(Parser *p)
{
    Program *prog = p->prog;
    if (prog->nfuncs == prog->capfuncs) {
        prog->capfuncs = prog->capfuncs ? prog->capfuncs * 2 : 8;
        prog->funcs = xrealloc(prog->funcs, prog->capfuncs * sizeof(Fn));
    }
    Fn *f = &prog->funcs[prog->nfuncs++];
    memset(f, 0, sizeof(*f));
    return f;
}

static StructDecl *add_struct(Parser *p)
{
    Program *prog = p->prog;
    if (prog->nstructs == prog->capstructs) {
        prog->capstructs = prog->capstructs ? prog->capstructs * 2 : 4;
        prog->structs = xrealloc(prog->structs,
                                 prog->capstructs * sizeof(StructDecl));
    }
    StructDecl *s = &prog->structs[prog->nstructs++];
    memset(s, 0, sizeof(*s));
    return s;
}

static int struct_index(Parser *p, const char *name)
{
    int i;
    for (i = 0; i < p->prog->nstructs; i++)
        if (strcmp(p->prog->structs[i].name, name) == 0)
            return i;
    return -1;
}

static Expr *parse_expr(Parser *p);
static int parse_int_literal(Parser *p);

/* type := ( '*'+ (int type | struct name) | '[' type ',' expr ']' )
 * (pointers wrap a base; array types are [element, length]).
 * allow_plain_struct controls whether a bare struct name is legal
 * (it is not for params/returns).
 */
static int parse_type(Parser *p, int allow_plain_struct)
{
    int stars = 0;
    while (at(p, T_STAR)) {
        next(p);
        stars++;
    }

    int base;
    if (at(p, T_LBRACK)) {
        SrcPos lp = peek(p)->pos;
        if (!allow_plain_struct)
            die_at(lp, "array types are not allowed for parameters or "
                   "return values in the MVP");
        next(p);
        base = parse_type(p, 1);
        expect(p, T_COMMA, "',' in array type");
        int len = parse_int_literal(p);
        if (len < 1)
            die_at(lp, "array length must be at least 1");
        expect(p, T_RBRACK, "']' to close array type");
        int type = array_type_of(p->prog, base, len);
        while (stars-- > 0)
            type = ptr_type_of(p->prog, type);
        return type;
    }

    Token *t = peek(p);
    if (t->kind != T_IDENT)
        die_at(t->pos, "expected a type name after '%s', found %s",
               stars > 0 ? "*" : "", tok_name(t->kind));

    int scalar = scalar_type_from_name(t->text);
    if (scalar) {
        base = scalar;
        next(p);
    } else {
        int idx = struct_index(p, t->text);
        if (idx < 0)
            die_at(t->pos, "unknown type '%s'", t->text);
        if (stars == 0 && !allow_plain_struct)
            die_at(t->pos, "type '%s' is a struct; a scalar or pointer type "
                   "is required here", t->text);
        base = T_STRUCT0 + idx;
        next(p);
    }

    int type = base;
    while (stars-- > 0)
        type = ptr_type_of(p->prog, type);
    return type;
}

/* cast targets: scalar integer names, optionally behind '*'s for
 * pointer reinterpretation */
static int parse_cast_type(Parser *p)
{
    int stars = 0;
    while (at(p, T_STAR)) {
        next(p);
        stars++;
    }
    Token *t = peek(p);
    if (t->kind != T_IDENT) {
        die_at(t->pos, "expected a type name after 'as', found %s",
               tok_name(t->kind));
    }
    int t_ = scalar_type_from_name(t->text);
    if (!t_ || t_ == T_VOID)
        die_at(t->pos, "'%s' is not a castable type name (use an integer "
               "type, or '*T' for pointers)", t->text);
    next(p);
    int type = t_;
    while (stars-- > 0)
        type = ptr_type_of(p->prog, type);
    return type;
}

static Expr *new_expr(Parser *p, ExprKind k, SrcPos pos);
static int parse_int_literal(Parser *p);
static Expr *parse_primary(Parser *p);
static Expr *parse_subscript(Parser *p);

/* ---- expressions ---- */

static Expr *new_expr(Parser *p, ExprKind k, SrcPos pos)
{
    (void)p;
    Expr *e = xmalloc(sizeof(Expr));
    memset(e, 0, sizeof(*e));
    e->kind = k;
    e->pos = pos;
    e->type = T_UNKNOWN;
    return e;
}

/* identifiers with field chains become variable-read lvalues */
static Expr *parse_lvalue_chain(Parser *p, Token *name)
{
    Expr *e = new_expr(p, E_LVAL, name->pos);
    e->u.lv.varname = name->text;
    e->u.lv.chain = NULL;
    e->u.lv.nchain = 0;
    e->u.lv.nstar = 0;
    e->u.lv.subs = NULL;
    e->u.lv.nsub = 0;
    while (at(p, T_DOT)) {
        next(p);
        Token *f = peek(p);
        if (f->kind != T_IDENT)
            die_at(f->pos, "expected field name after '.'");
        next(p);
        e->u.lv.chain = xrealloc(e->u.lv.chain,
                                 sizeof(char *) * (e->u.lv.nchain + 1));
        e->u.lv.chain[e->u.lv.nchain++] = f->text;
    }
    return e;
}

/* subscript: only allowed after an identifier (var or field chain). */
static Expr *parse_subscript(Parser *p)
{
    Expr *e = parse_primary(p);
    while (at(p, T_LBRACK)) {
        next(p);
        Expr *idx = parse_expr(p);
        expect(p, T_RBRACK, "']' to close subscript");
        Expr *s = new_expr(p, E_SUBSCRIPT, e->pos);
        s->u.sub.arr = e;
        s->u.sub.idx = idx;
        e = s;
    }
    return e;
}

/* ---------- primary ---------- */

static Expr *parse_primary(Parser *p)
{
    Token *t = peek(p);
    switch (t->kind) {
    case T_INT: {
        next(p);
        Expr *e = new_expr(p, E_INT, t->pos);
        e->u.ival = t->ival;
        return e;
    }
    case T_STR: {
        next(p);
        Expr *e = new_expr(p, E_STR, t->pos);
        e->u.str.bytes = t->text;
        e->u.str.len = (int)t->ival;
        return e;
    }
    case T_IDENT: {
        next(p);
        if (at(p, T_LPAREN)) {
            next(p);
            Expr *e = new_expr(p, E_CALL, t->pos);
            e->u.call.name = t->text;
            e->u.call.fni = -1;
            e->u.call.args = NULL;
            e->u.call.nargs = 0;
            while (!at(p, T_RPAREN)) {
                if (e->u.call.nargs > 0)
                    expect(p, T_COMMA, "',' between arguments");
                e->u.call.args = xrealloc(e->u.call.args,
                                          sizeof(Expr *) *
                                              (e->u.call.nargs + 1));
                e->u.call.args[e->u.call.nargs++] = parse_expr(p);
            }
            expect(p, T_RPAREN, "')' to close argument list");
            return e;
        }
        return parse_lvalue_chain(p, t);
    }
    case T_SIZEOF: {
        next(p);
        expect(p, T_LPAREN, "'(' after 'sizeof'");
        int ty = parse_type(p, 1);
        expect(p, T_RPAREN, "')' after sizeof type");
        Expr *e = new_expr(p, E_SIZEOF, t->pos);
        e->u.szof.ty = ty;
        return e;
    }
    case T_LPAREN: {
        next(p);
        Expr *e = parse_expr(p);
        expect(p, T_RPAREN, "')' to close expression");
        return e;
    }
    default:
        die_at(t->pos, "expected an expression, found %s", tok_name(t->kind));
    }
    return NULL;
}

/* unary level (with optional `as` casts applied to the operand) */
static Expr *parse_unary(Parser *p)
{
    Token *t = peek(p);
    switch (t->kind) {
    case T_MINUS:
    case T_PLUS: {
        next(p);
        Expr *a = parse_unary(p);
        if (t->kind == T_PLUS)
            return a;
        if (a->kind == E_INT) {
            a->u.ival = -a->u.ival;
            return a;
        }
        Expr *e = new_expr(p, E_UN, t->pos);
        e->u.un.op = T_MINUS;
        e->u.un.a = a;
        return e;
    }
    case T_NOT: {
        next(p);
        Expr *a = parse_unary(p);
        Expr *e = new_expr(p, E_UN, t->pos);
        e->u.un.op = T_NOT;
        e->u.un.a = a;
        return e;
    }
    case T_STAR: {
        next(p);
        Expr *a = parse_unary(p);
        Expr *e = new_expr(p, E_UN, t->pos);
        e->u.un.op = T_STAR; /* dereference */
        e->u.un.a = a;
        return e;
    }
    case T_AMP: {
        next(p);
        /* address-of: must apply to a var (possibly with fields) */
        Token *n = peek(p);
        if (n->kind != T_IDENT)
            die_at(n->pos, "expected a variable name after '&'");
        next(p);
        Expr *base = parse_lvalue_chain(p, n);
        Expr *e = new_expr(p, E_ADDR, t->pos);
        e->u.lv = base->u.lv;
        e->u.lv.pos = t->pos;
        free(base);
        return e;
    }
    default:
        break;
    }

    return parse_subscript(p);
}

/* `as` binds looser than unary operators: '*p as i32' is '(*p) as i32' */
static Expr *apply_casts(Parser *p, Expr *e)
{
    while (at(p, T_AS)) {
        next(p);
        int to = parse_cast_type(p);
        Expr *c = new_expr(p, E_CAST, e->pos);
        c->u.cast.a = e;
        c->u.cast.to = to;
        e = c;
    }
    return e;
}

static int parse_int_literal(Parser *p)
{
    Token *t = peek(p);
    if (t->kind != T_INT)
        die_at(t->pos, "expected a constant integer literal, found %s",
               tok_name(t->kind));
    next(p);
    return (int)t->ival;
}

static Expr *parse_mul(Parser *p)
{
    Expr *a = apply_casts(p, parse_unary(p));
    for (;;) {
        Token *t = peek(p);
        if (t->kind != T_STAR && t->kind != T_SLASH && t->kind != T_PERCENT)
            return a;
        next(p);
        Expr *b = parse_unary(p);
        Expr *e = new_expr(p, E_BIN, a->pos);
        e->u.bin.op = t->kind;
        e->u.bin.a = a;
        e->u.bin.b = b;
        a = e;
    }
}

static Expr *parse_add(Parser *p)
{
    Expr *a = parse_mul(p);
    for (;;) {
        Token *t = peek(p);
        if (t->kind != T_PLUS && t->kind != T_MINUS)
            return a;
        next(p);
        Expr *b = parse_mul(p);
        Expr *e = new_expr(p, E_BIN, a->pos);
        e->u.bin.op = t->kind;
        e->u.bin.a = a;
        e->u.bin.b = b;
        a = e;
    }
}

static Expr *parse_cmp(Parser *p)
{
    Expr *a = parse_add(p);
    Token *t = peek(p);
    switch (t->kind) {
    case T_EQ: case T_NE: case T_LT: case T_GT: case T_LE: case T_GE: {
        next(p);
        Expr *b = parse_add(p);
        Expr *e = new_expr(p, E_BIN, a->pos);
        e->u.bin.op = t->kind;
        e->u.bin.a = a;
        e->u.bin.b = b;
        return e;
    }
    default:
        return a;
    }
}

static Expr *parse_not(Parser *p)
{
    Token *t = peek(p);
    if (t->kind == T_NOT) {
        next(p);
        Expr *a = parse_not(p);
        Expr *e = new_expr(p, E_UN, t->pos);
        e->u.un.op = T_NOT;
        e->u.un.a = a;
        return e;
    }
    return parse_cmp(p);
}

static Expr *parse_and(Parser *p)
{
    Expr *a = parse_not(p);
    for (;;) {
        Token *t = peek(p);
        if (t->kind != T_AND)
            return a;
        next(p);
        Expr *b = parse_not(p);
        Expr *e = new_expr(p, E_BIN, a->pos);
        e->u.bin.op = T_AND;
        e->u.bin.a = a;
        e->u.bin.b = b;
        a = e;
    }
}

static Expr *parse_expr(Parser *p)
{
    Expr *a = parse_and(p);
    for (;;) {
        Token *t = peek(p);
        if (t->kind != T_OR)
            return a;
        next(p);
        Expr *b = parse_and(p);
        Expr *e = new_expr(p, E_BIN, a->pos);
        e->u.bin.op = T_OR;
        e->u.bin.a = a;
        e->u.bin.b = b;
        a = e;
    }
}

/* for [var] i : T = start to limit [by step] */
static Stmt *parse_for_stmt(Parser *p);
/* switch expr ... end */
static Stmt *parse_switch_stmt(Parser *p);

/* ---- statements ---- */

static Stmt *new_stmt(Parser *p, StmtKind k, SrcPos pos)
{
    (void)p;
    Stmt *s = xmalloc(sizeof(Stmt));
    memset(s, 0, sizeof(*s));
    s->kind = k;
    s->pos = pos;
    return s;
}

static Expr *parse_call_tail(Parser *p, Token *name)
{
    next(p); /* '(' */
    Expr *e = new_expr(p, E_CALL, name->pos);
    e->u.call.name = name->text;
    e->u.call.fni = -1;
    e->u.call.args = NULL;
    e->u.call.nargs = 0;
    while (!at(p, T_RPAREN)) {
        if (e->u.call.nargs > 0)
            expect(p, T_COMMA, "',' between arguments");
        e->u.call.args = xrealloc(e->u.call.args,
                                  sizeof(Expr *) * (e->u.call.nargs + 1));
        e->u.call.args[e->u.call.nargs++] = parse_expr(p);
    }
    expect(p, T_RPAREN, "')' to close argument list");
    return e;
}

static Stmt *parse_var_stmt(Parser *p)
{
    Token *start = next(p); /* 'var' */
    Token *name = peek(p);
    if (name->kind != T_IDENT)
        die_at(name->pos, "expected variable name after 'var'");
    next(p);
    expect(p, T_COLON, "':' after variable name");
    int type = parse_type(p, 1);
    Stmt *s = new_stmt(p, S_VAR, start->pos);
    s->u.vardecl.name = name->text;
    s->u.vardecl.type = type;
    s->u.vardecl.init = NULL;
    if (at(p, T_ASSIGN)) {
        next(p);
        s->u.vardecl.init = parse_expr(p);
    }
    return s;
}

static Stmt *parse_return_stmt(Parser *p)
{
    Token *start = next(p); /* 'return' */
    Stmt *s = new_stmt(p, S_RETURN, start->pos);
    if (!at(p, T_NL) && !at(p, T_EOF) &&
        !at(p, T_END) && !at(p, T_ELSE))
        s->u.ret.val = parse_expr(p);
    return s;
}

/* write through pointer: '*'+ IDENT '=' expr */
static Stmt *parse_deref_assign(Parser *p, SrcPos pos)
{
    int stars = 1;
    while (at(p, T_STAR)) {
        next(p);
        stars++;
    }
    Token *t = peek(p);
    if (t->kind != T_IDENT)
        die_at(t->pos, "expected a pointer variable after '*'", "");
    next(p);
    if (at(p, T_DOT))
        die_at(peek(p)->pos,
               "fields cannot be accessed through a dereference in this "
               "version");
    if (!at(p, T_ASSIGN))
        die_at(peek(p)->pos, "expected '=' after dereference target");
    next(p);
    Stmt *s = new_stmt(p, S_ASSIGN, pos);
    s->u.assign.lv.varname = t->text;
    s->u.assign.lv.chain = NULL;
    s->u.assign.lv.nchain = 0;
    s->u.assign.lv.nstar = stars;
    s->u.assign.lv.subs = NULL;
    s->u.assign.lv.nsub = 0;
    s->u.assign.val = parse_expr(p);
    return s;
}

/* assignment whose target is an identifier with fields, or a call */
static Stmt *parse_assignment_target(Parser *p, Token *first)
{
    next(p); /* consume the identifier */
    Stmt *s = new_stmt(p, S_ASSIGN, first->pos);
    s->u.assign.lv.varname = first->text;
    s->u.assign.lv.chain = NULL;
    s->u.assign.lv.nchain = 0;
    s->u.assign.lv.nstar = 0;

    if (at(p, T_LPAREN)) {
        Stmt *c = new_stmt(p, S_EXPR, first->pos);
        c->u.estmt.call = parse_call_tail(p, first);
        return c;
    }
    while (at(p, T_DOT)) {
        next(p);
        Token *f = peek(p);
        if (f->kind != T_IDENT)
            die_at(f->pos, "expected field name after '.'");
        next(p);
        s->u.assign.lv.chain =
            xrealloc(s->u.assign.lv.chain,
                     sizeof(char *) * (s->u.assign.lv.nchain + 1));
        s->u.assign.lv.chain[s->u.assign.lv.nchain++] = f->text;
    }
    while (at(p, T_LBRACK)) {
        next(p);
        Expr *ix = parse_expr(p);
        expect(p, T_RBRACK, "']' to close subscript");
        s->u.assign.lv.subs = xrealloc(s->u.assign.lv.subs,
                                       sizeof(Expr *) *
                                           (s->u.assign.lv.nsub + 1));
        s->u.assign.lv.subs[s->u.assign.lv.nsub++] = ix;
    }
    if (!at(p, T_ASSIGN)) {
        Token *t = peek(p);
        die_at(t->pos,
               "expected '=', '[' or a call after identifier '%s', found %s%s%s%s",
               first->text, tok_name(t->kind),
               t->kind == T_IDENT ? "'" : "",
               t->kind == T_IDENT ? t->text : "",
               t->kind == T_IDENT ? "'" : "");
    }
    next(p);
    s->u.assign.val = parse_expr(p);
    return s;
}

static void parse_stmt_list(Parser *p, Stmt ***list, int *n)
{
    /* (parse_for_stmt is defined after this function) */
    *list = NULL;
    *n = 0;
    for (;;) {
        while (at(p, T_NL))
            next(p);
        Token *t = peek(p);
        if (t->kind == T_END || t->kind == T_ELSE || t->kind == T_EOF ||
            t->kind == T_CASE || t->kind == T_DEFAULT)
            return;

        Stmt *s = NULL;
        switch (t->kind) {
        case T_VAR:
            s = parse_var_stmt(p);
            break;
        case T_RETURN:
            s = parse_return_stmt(p);
            break;
        case T_BREAK:
            next(p);
            s = new_stmt(p, S_BREAK, t->pos);
            break;
        case T_CONTINUE:
            next(p);
            s = new_stmt(p, S_CONTINUE, t->pos);
            break;
        case T_IF: {
            Token *kw = next(p);
            Expr *cond = parse_expr(p);
            if (!at(p, T_NL))
                die_at(peek(p)->pos,
                       "expected end of line after 'if' condition");
            expect_nl(p);
            Stmt **t2 = NULL, **e2 = NULL;
            int nt, ne;
            parse_stmt_list(p, &t2, &nt);
            ne = 0;
            if (at(p, T_ELSE)) {
                next(p);
                if (!at(p, T_NL))
                    die_at(peek(p)->pos,
                           "expected end of line after 'else'");
                expect_nl(p);
                parse_stmt_list(p, &e2, &ne);
            }
            expect(p, T_END, "'end' to close if statement");
            s = new_stmt(p, S_IF, kw->pos);
            s->u.ifs.cond = cond;
            s->u.ifs.then = t2;
            s->u.ifs.nthen = nt;
            s->u.ifs.els = e2;
            s->u.ifs.nels = ne;
            break;
        }
        case T_WHILE: {
            Token *kw = next(p);
            Expr *cond = parse_expr(p);
            if (!at(p, T_NL))
                die_at(peek(p)->pos,
                       "expected end of line after 'while' condition");
            expect_nl(p);
            Stmt **body = NULL;
            int nb;
            parse_stmt_list(p, &body, &nb);
            expect(p, T_END, "'end' to close while statement");
            s = new_stmt(p, S_WHILE, kw->pos);
            s->u.whiles.cond = cond;
            s->u.whiles.body = body;
            s->u.whiles.n = nb;
            break;
        }
        case T_FOR:
            s = parse_for_stmt(p);
            break;
        case T_SWITCH:
            s = parse_switch_stmt(p);
            break;
        case T_IDENT:
            s = parse_assignment_target(p, t);
            break;
        case T_STAR:
            next(p);
            s = parse_deref_assign(p, t->pos);
            break;
        default:
            die_at(t->pos,
                   "a statement must start with var, return, if, while, for, "
                   "switch, break, continue, an assignment or a function call "
                   "(found %s)", tok_name(t->kind));
            return; /* unreachable */
        }

        *list = xrealloc(*list, sizeof(Stmt *) * (*n + 1));
        (*list)[(*n)++] = s;

        if (!at(p, T_NL) && !at(p, T_END) && !at(p, T_ELSE) &&
            !at(p, T_EOF)) {
            Token *q = peek(p);
            die_at(q->pos, "expected end of line after statement, found %s",
                   tok_name(q->kind));
        }
    }
}

/* for [var] i : T = start to limit [by step] */
static Stmt *parse_for_stmt(Parser *p)
{
    Token *kw = next(p); /* 'for' */
    int has_var = 0;
    Token *name;
    if (at(p, T_VAR)) {
        next(p); /* consume 'var' */
        has_var = 1;
        name = peek(p);
        if (name->kind != T_IDENT)
            die_at(name->pos, "expected variable name after 'var'");
        next(p);
    } else {
        name = peek(p);
        if (name->kind != T_IDENT)
            die_at(name->pos, "expected 'var' or variable name after 'for'");
        next(p);
    }
    expect(p, T_COLON, "':' after variable name in 'for'");
    int type = parse_type(p, 1);
    expect(p, T_ASSIGN, "'=' after type in 'for'");
    Expr *start = parse_expr(p);
    expect(p, T_TO, "'to' after start expression in 'for'");
    Expr *limit = parse_expr(p);
    Expr *step = NULL;
    if (at(p, T_BY)) {
        next(p);
        step = parse_expr(p);
    }
    if (!at(p, T_NL))
        die_at(peek(p)->pos, "expected end of line after 'for' header");
    expect_nl(p);
    Stmt **body = NULL;
    int nb;
    parse_stmt_list(p, &body, &nb);
    expect(p, T_END, "'end' to close 'for' statement");
    Stmt *s = new_stmt(p, S_FOR, kw->pos);
    s->u.fors.varname = name->text;
    s->u.fors.type = type;
    s->u.fors.start = start;
    s->u.fors.limit = limit;
    s->u.fors.step = step;
    s->u.fors.body = body;
    s->u.fors.nbody = nb;
    s->u.fors.has_var = has_var;
    return s;
}

/* switch expr \n case lit \n stmts ... end */
static Stmt *parse_switch_stmt(Parser *p)
{
    Token *kw = next(p);  /* 'switch' */
    Expr *expr = parse_expr(p);
    if (!at(p, T_NL))
        die_at(peek(p)->pos, "expected end of line after switch expression");
    expect_nl(p);

    SwitchCase *cases = NULL;
    int ncases = 0;
    int ccap = 0;
    Stmt **defbody = NULL;
    int ndef = 0;
    int has_default = 0;

    while (!at(p, T_END) && !at(p, T_EOF)) {
        if (at(p, T_CASE)) {
            next(p);  /* 'case' */
            Token *lit = peek(p);
            if (lit->kind != T_INT && lit->kind != T_STR)
                die_at(lit->pos, "case value must be an integer or string literal");
            long long val = 0;
            if (lit->kind == T_INT) {
                val = lit->ival;
                next(p);
            } else {
                /* single-char string as integer */
                if (lit->text && lit->text[0] && lit->text[1] == 0)
                    val = (unsigned char)lit->text[0];
                else
                    die_at(lit->pos, "case string must be a single character");
                next(p);
            }
            if (!at(p, T_NL))
                die_at(peek(p)->pos, "expected end of line after case value");
            expect_nl(p);
            Stmt **body = NULL;
            int nb = 0;
            parse_stmt_list(p, &body, &nb);
            if (ncases >= ccap) {
                ccap = ccap ? ccap * 2 : 8;
                cases = xrealloc(cases, sizeof(SwitchCase) * (size_t)ccap);
            }
            cases[ncases].val = val;
            cases[ncases].body = body;
            cases[ncases].nbody = nb;
            ncases++;
        } else if (at(p, T_DEFAULT)) {
            next(p);  /* 'default' */
            if (has_default)
                die_at(kw->pos, "multiple default cases in switch");
            has_default = 1;
            if (!at(p, T_NL))
                die_at(peek(p)->pos, "expected end of line after 'default'");
            expect_nl(p);
            parse_stmt_list(p, &defbody, &ndef);
        } else {
            die_at(peek(p)->pos, "expected 'case' or 'default' in switch body, found %s",
                   tok_name(peek(p)->kind));
        }
    }
    expect(p, T_END, "'end' to close switch statement");
    expect_nl(p);

    /* build AST node */
    Stmt *s = new_stmt(p, S_SWITCH, kw->pos);
    s->u.sw.expr = expr;

    /* append default as last case with val = -1 */
    if (has_default) {
        if (ncases >= ccap) {
            ccap = ccap ? ccap * 2 : 8;
            cases = xrealloc(cases, sizeof(SwitchCase) * (size_t)ccap);
        }
        cases[ncases].val = -1;
        cases[ncases].body = defbody;
        cases[ncases].nbody = ndef;
        ncases++;
    }
    s->u.sw.cases = cases;
    s->u.sw.ncases = ncases;
    return s;
}

/* ---- toplevel ---- */

static void parse_const_decl(Parser *p)
{
    Token *kw = next(p); /* 'const' */
    Token *name = peek(p);
    if (name->kind != T_IDENT)
        die_at(name->pos, "expected constant name after 'const'");
    next(p);
    int i;
    for (i = 0; i < p->prog->nconsts; i++)
        if (strcmp(p->prog->consts[i].name, name->text) == 0)
            die_at(name->pos, "duplicate constant '%s'", name->text);
    expect(p, T_COLON, "':' after constant name");
    int type = parse_type(p, 0);
    if (!type_is_int(type))
        die_at(name->pos, "constant '%s' must have an integer type in the "
               "MVP", name->text);
    expect(p, T_ASSIGN, "'=' and an integer literal");
    Token *lit = peek(p);
    if (lit->kind != T_INT)
        die_at(lit->pos, "constant initializer must be an integer literal");
    next(p);
    if (p->prog->nconsts == p->prog->capconsts) {
        p->prog->capconsts = p->prog->capconsts ? p->prog->capconsts * 2 : 4;
        p->prog->consts = xrealloc(p->prog->consts,
                                   sizeof(ConstDecl) * p->prog->capconsts);
    }
    ConstDecl *c = &p->prog->consts[p->prog->nconsts++];
    c->name = name->text;
    c->type = type;
    c->val = lit->ival;
    (void)kw;
}

static void parse_global_decl(Parser *p)
{
    Token *kw = next(p); /* 'global' */
    Token *name = peek(p);
    if (name->kind != T_IDENT)
        die_at(name->pos, "expected variable name after 'global'");
    next(p);
    int i;
    for (i = 0; i < p->prog->nglobals; i++)
        if (strcmp(p->prog->globals[i].name, name->text) == 0)
            die_at(name->pos, "duplicate global '%s'", name->text);
    expect(p, T_COLON, "':' after global variable name");
    int type = parse_type(p, 1);
    Expr *init = NULL;
    if (at(p, T_ASSIGN)) {
        next(p);
        init = parse_expr(p);
    }
    if (p->prog->nglobals == p->prog->capglobals) {
        p->prog->capglobals = p->prog->capglobals ? p->prog->capglobals * 2 : 4;
        p->prog->globals = xrealloc(p->prog->globals,
                                    sizeof(GlobalDecl) * p->prog->capglobals);
    }
    GlobalDecl *g = &p->prog->globals[p->prog->nglobals++];
    g->name = name->text;
    g->type = type;
    g->init = init;
    (void)kw;
}

static void parse_struct_decl(Parser *p)
{
    Token *kw = next(p); /* 'struct' */
    Token *name = peek(p);
    if (name->kind != T_IDENT)
        die_at(name->pos, "expected struct name after 'struct'");
    if (struct_index(p, name->text) >= 0)
        die_at(name->pos, "duplicate struct '%s'", name->text);
    next(p);
    if (!at(p, T_NL))
        die_at(peek(p)->pos, "expected end of line after struct name");
    expect_nl(p);

    StructDecl *s = add_struct(p);
    s->name = name->text;
    s->type = T_STRUCT0 + (p->prog->nstructs - 1);
    s->fields = NULL;
    s->nf = 0;

    for (;;) {
        while (at(p, T_NL)) {
            next(p);
            continue;
        }
        if (at(p, T_END))
            break;
        if (at(p, T_EOF))
            die_at(peek(p)->pos, "unexpected end of file inside struct '%s'",
                   s->name);
        Token *fname = peek(p);
        if (fname->kind != T_IDENT)
            die_at(fname->pos,
                   "expected field declaration or 'end', found %s",
                   tok_name(fname->kind));
        next(p);
        expect(p, T_COLON, "':' after field name");
        int type = parse_type(p, 1);
        if (!at(p, T_NL))
            die_at(peek(p)->pos,
                   "expected end of line after field declaration");
        expect_nl(p);
        s->fields = xrealloc(s->fields, sizeof(FieldDecl) * (s->nf + 1));
        s->fields[s->nf].name = fname->text;
        s->fields[s->nf].type = type;
        s->nf++;
    }
    expect(p, T_END, "'end' to close struct declaration");
    (void)kw;
}

static void parse_fn_decl(Parser *p)
{
    Token *kw = next(p); /* 'fn' */
    Token *name = peek(p);
    if (name->kind != T_IDENT)
        die_at(name->pos, "expected function name after 'fn'");
    if (strncmp(name->text, "swl_", 4) == 0)
        die_at(name->pos, "function name '%s' uses the reserved 'swl_' "
               "prefix", name->text);
    if (fn_by_name(p->prog, name->text))
        die_at(name->pos, "duplicate function '%s'", name->text);
    next(p);

    expect(p, T_LPAREN, "'(' after function name");
    Fn *f = add_fn(p);
    f->name = name->text;
    f->ret = T_VOID;
    f->params = NULL;
    f->nparams = 0;
    if (!at(p, T_RPAREN)) {
        for (;;) {
            Token *pn = peek(p);
            if (pn->kind != T_IDENT)
                die_at(pn->pos, "expected parameter name");
            next(p);
            expect(p, T_COLON, "':' after parameter name");
            /* params may be scalars, pointers, or plain structs (by value) */
            int type = parse_type(p, 1);
            f->params = xrealloc(f->params,
                                 sizeof(Param) * (f->nparams + 1));
            f->params[f->nparams].name = pn->text;
            f->params[f->nparams].type = type;
            f->nparams++;
            if (at(p, T_COMMA)) {
                next(p);
                continue;
            }
            break;
        }
    }
    expect(p, T_RPAREN, "')' to close parameter list");
    if (at(p, T_ARROW)) {
        next(p);
        f->ret = parse_type(p, 1);  /* allow struct return by value */
    }
    if (!at(p, T_NL))
        die_at(peek(p)->pos,
               "expected end of line after function signature");
    expect_nl(p);
    parse_stmt_list(p, &f->body, &f->nbody);
    expect(p, T_END, "'end' to close function '%s'", f->name);
    (void)kw;
}

void parse_program(Token *toks, int ntok, Program *p)
{
    Parser ps;
    Parser *p_ = &ps;
    memset(p_, 0, sizeof(*p_));
    p_->t = toks;
    p_->n = ntok;
    p_->i = 0;
    p_->prog = p;

    while (at(p_, T_NL))
        next(p_);

    Token *m = peek(p_);
    if (m->kind != T_MODULE) {
        die_at(m->pos, "a SWL file must start with 'module <name>' "
               "(found %s)", tok_name(m->kind));
    }
    next(p_);
    Token *mn = peek(p_);
    if (mn->kind != T_IDENT)
        die_at(mn->pos, "expected module name after 'module'");
    next(p_);
    p->name = mn->text;
    if (!at(p_, T_NL))
        die_at(peek(p_)->pos, "expected end of line after module name");
    expect_nl(p_);

    for (;;) {
        while (at(p_, T_NL))
            next(p_);
        Token *t = peek(p_);
        if (t->kind == T_EOF)
            break;
        if (t->kind == T_MODULE)
            die_at(t->pos, "only one 'module' per file is supported in "
                   "the MVP");
        if (t->kind == T_STRUCT) {
            parse_struct_decl(p_);
            if (!at(p_, T_NL))
                die_at(peek(p_)->pos,
                       "expected end of line after struct declaration");
            expect_nl(p_);
        } else if (t->kind == T_CONST) {
            parse_const_decl(p_);
            if (!at(p_, T_NL))
                die_at(peek(p_)->pos,
                       "expected end of line after constant declaration");
            expect_nl(p_);
        } else if (t->kind == T_GLOBAL) {
            parse_global_decl(p_);
            if (!at(p_, T_NL))
                die_at(peek(p_)->pos,
                       "expected end of line after global declaration");
            expect_nl(p_);
        } else if (t->kind == T_FN) {
            parse_fn_decl(p_);
            if (!at(p_, T_NL))
                die_at(peek(p_)->pos,
                       "expected end of line after function declaration");
            expect_nl(p_);
        } else {
            die_at(t->pos, "expected 'struct', 'const', 'global' or 'fn' at top "
                   "level, found %s", tok_name(t->kind));
        }
    }
}
