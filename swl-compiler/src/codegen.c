/*
 * codegen.c -- x86 32-bit code generator.
 *
 * Emits NASM-compatible assembly for the SWL MVP subset:
 *
 *   - cdecl-like convention: arguments are pushed right-to-left and popped
 *     by the caller; parameters live at [ebp+8+4*i]; the return value is
 *     left in eax.
 *   - every function has a full frame: push ebp / mov ebp,esp / sub esp,FRAME.
 *     The local area is zeroed in the prologue, so locals start at 0
 *     (pointers start NULL, string/char data lives in .data).
 *   - expression evaluation is a simple stack machine with the result in eax.
 *   - after every arithmetic operation the result is re-normalized
 *     (sign/zero extended) to its SWL type; i8/i16 arithmetic is exact.
 *   - i8/i16 loads extend to 32 bits by signedness; u* use unsigned div and
 *     comparisons.
 *   - pointers are plain 32-bit values: `&lv` materializes an address
 *     (lea), `*p` loads/stores through [eax], `*p = v` writes through it.
 *
 * `fn main` is emitted as `swl_main`; every other function is
 * `swl_fn_<name>`; string literals become `swl_str_N` in .data.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swlc.h"

/* ------------------------------------------------------------------ */
/* Output buffer                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    char *s;
    int len;
    int cap;
} Buf;

static void bput(Buf *b, const char *s)
{
    int n = (int)strlen(s);
    if (b->len + n + 1 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4096;
        while (b->len + n + 1 > b->cap)
            b->cap *= 2;
        b->s = xrealloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void bfmt(Buf *b, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < (int)sizeof(tmp)) {
        bput(b, tmp);
    } else {
        char *big = xmalloc(n + 1);
        va_start(ap, fmt);
        vsnprintf(big, n + 1, fmt, ap);
        va_end(ap);
        bput(b, big);
        free(big);
    }
}

/* ------------------------------------------------------------------ */
/* Code generation context                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    Program *p;
    Fn *fn;
    Buf *out;
    int lbl;
    int loop_cont;  /* label of the current while condition (-1 outside) */
    int loop_end;   /* label after the current while */
} Ctx;

static int new_label(Ctx *c)
{
    return c->lbl++;
}

static void emit_mem_ebp(Buf *b, int disp)
{
    if (disp >= 0)
        bfmt(b, "[ebp+%d]", disp);
    else
        bfmt(b, "[ebp-%d]", -disp);
}

/* eax already holds a value of SWL type t; re-normalize it in place. */
static void gen_norm(Ctx *c, int t)
{
    switch (t) {
    case T_I8:  bfmt(c->out, "    movsx eax, al\n"); break;
    case T_U8:  bfmt(c->out, "    movzx eax, al\n"); break;
    case T_I16: bfmt(c->out, "    movsx eax, ax\n"); break;
    case T_U16: bfmt(c->out, "    movzx eax, ax\n"); break;
    default: break;
    }
}

/* convert the i32 value in eax to type t (explicit `as` conversion):
 * truncate to the target width, then extend per the target's signedness */
static void gen_conv(Ctx *c, int t)
{
    switch (t) {
    case T_I8:  bfmt(c->out, "    movsx eax, al\n"); break;
    case T_U8:  bfmt(c->out, "    movzx eax, al\n"); break;
    case T_I16: bfmt(c->out, "    movsx eax, ax\n"); break;
    case T_U16: bfmt(c->out, "    movzx eax, ax\n"); break;
    default:    break; /* 32-bit targets need no work */
    }
}

/* load the value at [ebp+disp] (type t) into eax, extended to 32 bits */
#define GLOBAL_DISP (-999999999)

static void gen_load_offs(Ctx *c, int t, int disp)
{
    Buf *b = c->out;
    bfmt(b, "    %s eax, ", t == T_I8 ? "movsx" : t == T_U8 ? "movzx"
             : t == T_I16 ? "movsx" : t == T_U16 ? "movzx" : "mov");
    if (type_size(t) == 1)
        bput(b, "byte ");
    else if (type_size(t) == 2)
        bput(b, "word ");
    else
        bput(b, "dword ");
    emit_mem_ebp(b, disp);
    bput(b, "\n");
}

/* load from a global variable by name */
static void gen_load_global(Ctx *c, int t, const char *name)
{
    Buf *b = c->out;
    bfmt(b, "    %s eax, ", t == T_I8 ? "movsx" : t == T_U8 ? "movzx"
             : t == T_I16 ? "movsx" : t == T_U16 ? "movzx" : "mov");
    if (type_size(t) == 1)
        bfmt(b, "byte [swl_g_%s]\n", name);
    else if (type_size(t) == 2)
        bfmt(b, "word [swl_g_%s]\n", name);
    else
        bfmt(b, "dword [swl_g_%s]\n", name);
}

/* load the value pointed to by eax (type t) into eax */
static void gen_load_deref(Ctx *c, int t)
{
    Buf *b = c->out;
    bfmt(b, "    %s eax, ", t == T_I8 ? "movsx" : t == T_U8 ? "movzx"
             : t == T_I16 ? "movsx" : t == T_U16 ? "movzx" : "mov");
    if (type_size(t) == 1)
        bput(b, "byte [eax]\n");
    else if (type_size(t) == 2)
        bput(b, "word [eax]\n");
    else
        bput(b, "dword [eax]\n");
}

/* store the low bits of eax to [ebp+disp], typed t */
static void gen_store_offs(Ctx *c, int t, int disp)
{
    Buf *b = c->out;
    if (type_size(t) == 1)
        bfmt(b, "    mov byte ");
    else if (type_size(t) == 2)
        bfmt(b, "    mov word ");
    else
        bfmt(b, "    mov dword ");
    emit_mem_ebp(b, disp);
    if (type_size(t) == 1)
        bput(b, ", al\n");
    else if (type_size(t) == 2)
        bput(b, ", ax\n");
    else
        bput(b, ", eax\n");
}

/* store eax to a global variable by name */
static void gen_store_global(Ctx *c, int t, const char *name)
{
    Buf *b = c->out;
    if (type_size(t) == 1)
        bfmt(b, "    mov byte [swl_g_%s], al\n", name);
    else if (type_size(t) == 2)
        bfmt(b, "    mov word [swl_g_%s], ax\n", name);
    else
        bfmt(b, "    mov dword [swl_g_%s], eax\n", name);
}

/* store ecx (low bits) through the address in eax, typed t */
static void gen_store_deref(Ctx *c, int t)
{
    Buf *b = c->out;
    if (type_size(t) == 1)
        bput(b, "    mov byte [eax], cl\n");
    else if (type_size(t) == 2)
        bput(b, "    mov word [eax], cx\n");
    else
        bput(b, "    mov dword [eax], ecx\n");
}

/* copy 'size' bytes from [ecx] (source) to [eax] (dest).
 * Used for struct assignment and struct initialization. */
static void gen_struct_memcpy(Buf *b, int size)
{
    int i;
    for (i = 0; i + 4 <= size; i += 4)
        bfmt(b, "    mov edx, dword [ecx + %d]\n"
                "    mov dword [eax + %d], edx\n", i, i);
    /* copy remaining bytes (0..3) */
    for (; i < size; i++)
        bfmt(b, "    mov dl, byte [ecx + %d]\n"
                "    mov byte [eax + %d], dl\n", i, i);
}

static VarSym *var_of(Ctx *c, const char *name)
{
    VarSym *v;
    for (v = c->fn->vars; v; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* String literals                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char *bytes;
    int len;
} StrDef;

typedef struct {
    StrDef *defs;
    int ndefs;
    int cap;
} StrTable;

/* returns the swl_str_N label index for the literal */
static int str_index(StrTable *t, const char *bytes, int len)
{
    int i;
    for (i = 0; i < t->ndefs; i++)
        if (t->defs[i].len == len &&
            memcmp(t->defs[i].bytes, bytes, len) == 0)
            return i;
    if (t->ndefs == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->defs = xrealloc(t->defs, t->cap * sizeof(StrDef));
    }
    char *copy = xmalloc(len + 1);
    memcpy(copy, bytes, len);
    copy[len] = '\0';
    t->defs[t->ndefs].bytes = copy;
    t->defs[t->ndefs].len = len;
    return t->ndefs++;
}

/* ------------------------------------------------------------------ */
/* Expressions                                                         */
/* ------------------------------------------------------------------ */

static void gen_expr(Ctx *c, Expr *e, StrTable *strs);

/* Compute in eax the address of the element selected by a subscript
 * expression.  Recursion makes `m[i][j]` work: the inner node's address
 * is the outer node's base.  A plain array lvalue contributes its slot
 * address (lea); anything else must already hold an address value
 * (pointer variable, string literal, decayed array, ...). */
static void gen_addr_of(Ctx *c, Expr *e, StrTable *strs)
{
    Buf *b = c->out;
    if (e->kind == E_SUBSCRIPT) {
        gen_addr_of(c, e->u.sub.arr, strs);
        bput(b, "    push eax\n");
        gen_expr(c, e->u.sub.idx, strs);
        bput(b, "    mov ecx, eax\n");
        int sz = type_size(e->type);
        if (sz > 1)
            bfmt(b, "    imul ecx, ecx, %d\n", sz);
        bput(b, "    pop eax\n"
                "    add eax, ecx\n");
        return;
    }
    if (e->kind == E_LVAL && type_is_array(e->u.lv.type)) {
        bfmt(b, "    lea eax, ");
        emit_mem_ebp(b, e->u.lv.disp);
        bput(b, "\n");
        return;
    }
    gen_expr(c, e, strs); /* value already holds an address */
}

static void gen_call(Ctx *c, Expr *e, StrTable *strs)
{
    int i;
    for (i = e->u.call.nargs - 1; i >= 0; i--) {
        gen_expr(c, e->u.call.args[i], strs);
        bput(c->out, "    push eax\n");
    }
    if (e->u.call.fni >= 0) {
        Fn *f = &c->p->funcs[e->u.call.fni];
        if (strcmp(f->name, "main") == 0)
            bput(c->out, "    call swl_main\n");
        else
            bfmt(c->out, "    call swl_fn_%s\n", f->name);
    } else {
        bfmt(c->out, "    call %s\n", e->u.call.name);
    }
    if (e->u.call.nargs > 0)
        bfmt(c->out, "    add esp, %d\n", 4 * e->u.call.nargs);
}

static void gen_expr(Ctx *c, Expr *e, StrTable *strs)
{
    Buf *b = c->out;

    switch (e->kind) {
    case E_INT:
    case E_CONST: {
        long long v = e->kind == E_INT ? e->u.ival : e->u.cst.val;
        bfmt(b, "    mov eax, 0x%08llX\n",
             (unsigned long long)(v & 0xFFFFFFFFLL));
        break;
    }
    case E_LVAL:
        if (type_is_struct(e->u.lv.type)) {
            /* struct value: load address (like E_ADDR) */
            if (e->u.lv.disp == GLOBAL_DISP)
                bfmt(b, "    mov eax, swl_g_%s\n", e->u.lv.varname);
            else {
                bfmt(b, "    lea eax, ");
                emit_mem_ebp(b, e->u.lv.disp);
                bput(b, "\n");
            }
        } else if (e->u.lv.disp == GLOBAL_DISP)
            gen_load_global(c, e->u.lv.type, e->u.lv.varname);
        else
            gen_load_offs(c, e->u.lv.type, e->u.lv.disp);
        break;
    case E_ADDR:
        if (e->u.lv.disp == GLOBAL_DISP)
            bfmt(b, "    mov eax, swl_g_%s\n", e->u.lv.varname);
        else {
            bfmt(b, "    lea eax, ");
            emit_mem_ebp(b, e->u.lv.disp);
            bput(b, "\n");
        }
        break;
    case E_STR: {
        int idx = str_index(strs, e->u.str.bytes, e->u.str.len);
        bfmt(b, "    mov eax, swl_str_%d\n", idx);
        break;
    }
    case E_CALL:
        gen_call(c, e, strs);
        break;
    case E_CAST:
        gen_expr(c, e->u.cast.a, strs);
        gen_conv(c, e->u.cast.to);
        break;
    case E_BIN: {
        int op = e->u.bin.op;

        if (op == T_AND) {
            int lzero = new_label(c);
            int lend = new_label(c);
            gen_expr(c, e->u.bin.a, strs);
            bfmt(b, "    test eax, eax\n"
                    "    jz L%d\n", lzero);
            gen_expr(c, e->u.bin.b, strs);
            bfmt(b, "    test eax, eax\n"
                    "    jz L%d\n"
                    "    mov eax, 1\n"
                    "    jmp L%d\n"
                    "L%d:\n"
                    "    xor eax, eax\n"
                    "L%d:\n", lzero, lend, lzero, lend);
            break;
        }
        if (op == T_OR) {
            int lone = new_label(c);
            int lend = new_label(c);
            gen_expr(c, e->u.bin.a, strs);
            bfmt(b, "    test eax, eax\n"
                    "    jnz L%d\n", lone);
            gen_expr(c, e->u.bin.b, strs);
            bfmt(b, "    test eax, eax\n"
                    "    jnz L%d\n"
                    "    xor eax, eax\n"
                    "    jmp L%d\n"
                    "L%d:\n"
                    "    mov eax, 1\n"
                    "L%d:\n", lone, lend, lone, lend);
            break;
        }

        int signed_op = type_is_signed(e->u.bin.a->type);
        int arith = op == T_PLUS || op == T_MINUS || op == T_STAR ||
                    op == T_SLASH || op == T_PERCENT;

        gen_expr(c, e->u.bin.a, strs);
        bput(b, "    push eax\n");
        gen_expr(c, e->u.bin.b, strs);
        bput(b, "    pop ecx\n"
                "    xchg eax, ecx\n");

        if (arith) {
            if (e->u.bin.scale > 0) {
                /* pointer arithmetic: eax = a, ecx = b -- scale the
                 * integer operand by the base size */
                if (type_is_int(e->u.bin.a->type))
                    bfmt(b, "    imul eax, eax, %d\n", e->u.bin.scale);
                else
                    bfmt(b, "    imul ecx, ecx, %d\n", e->u.bin.scale);
            }
            switch (op) {
            case T_PLUS:  bput(b, "    add eax, ecx\n"); break;
            case T_MINUS: bput(b, "    sub eax, ecx\n"); break;
            case T_STAR:  bput(b, "    imul eax, ecx\n"); break;
            case T_SLASH:
            case T_PERCENT:
                if (signed_op)
                    bput(b, "    cdq\n    idiv ecx\n");
                else
                    bput(b, "    xor edx, edx\n    div ecx\n");
                if (op == T_PERCENT)
                    bput(b, "    mov eax, edx\n");
                break;
            }
            gen_norm(c, e->type);
        } else {
            const char *cc = NULL;
            switch (op) {
            case T_EQ: cc = "sete"; break;
            case T_NE: cc = "setne"; break;
            case T_LT: cc = signed_op ? "setl" : "setb"; break;
            case T_GT: cc = signed_op ? "setg" : "seta"; break;
            case T_LE: cc = signed_op ? "setle" : "setbe"; break;
            case T_GE: cc = signed_op ? "setge" : "setae"; break;
            }
            bfmt(b, "    cmp eax, ecx\n    %s al\n    movzx eax, al\n", cc);
        }
        break;
    }
    case E_UN:
        switch (e->u.un.op) {
        case T_MINUS:
            gen_expr(c, e->u.un.a, strs);
            bput(b, "    neg eax\n");
            gen_norm(c, e->type);
            break;
        case T_NOT:
            gen_expr(c, e->u.un.a, strs);
            bput(b, "    test eax, eax\n"
                    "    setz al\n"
                    "    movzx eax, al\n");
            break;
        case T_STAR: /* dereference read */
            gen_expr(c, e->u.un.a, strs);
            gen_load_deref(c, e->type);
            break;
        }
        break;
    case E_SUBSCRIPT:
        gen_addr_of(c, e, strs);
        gen_load_deref(c, e->type);
        break;
    case E_SIZEOF: {
        int sz = type_size_of(c->p, e->u.szof.ty);
        bfmt(b, "    mov eax, %d\n", sz);
        break;
    }
    }
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

static void gen_stmt(Ctx *c, Stmt *s, int retlabel, StrTable *strs)
{
    Buf *b = c->out;

    switch (s->kind) {
    case S_VAR: {
        if (s->u.vardecl.init) {
            Expr *in = s->u.vardecl.init;
            if (type_is_array(s->u.vardecl.type) && in->kind == E_STR) {
                /* var buf: [u8, N] = "..." -> store the bytes (and the
                 * zero padding) directly into the reserved slot. */
                VarSym *v = var_of(c, s->u.vardecl.name);
                int d = v->disp;
                int alen = array_len(s->u.vardecl.type);
                int blen = in->u.str.len;
                int i;
                for (i = 0; i < alen; i++) {
                    unsigned char bytev = i < blen
                        ? (unsigned char)in->u.str.bytes[i] : 0;
                    bfmt(b, "    mov byte [ebp%d], 0x%02X\n", d + i, bytev);
                }
                break;
            }
            if (type_is_struct(s->u.vardecl.type)) {
                /* struct init: var s: Pair = expr
                 * gen_expr loads address of source into eax.
                 * We need to compute address of dest and memcpy. */
                gen_expr(c, in, strs);
                bput(b, "    push eax\n"); /* source address */
                VarSym *v = var_of(c, s->u.vardecl.name);
                bfmt(b, "    lea eax, ");
                emit_mem_ebp(b, v->disp);
                bput(b, "\n    pop ecx\n");
                gen_struct_memcpy(b, type_size_of(c->p, s->u.vardecl.type));
            } else {
                gen_expr(c, in, strs);
                VarSym *v = var_of(c, s->u.vardecl.name);
                gen_store_offs(c, s->u.vardecl.type, v->disp);
            }
        }
        break;
    }
    case S_RETURN:
        if (s->u.ret.val)
            gen_expr(c, s->u.ret.val, strs);
        bfmt(b, "    jmp L%d\n", retlabel);
        break;
    case S_IF: {
        int lelse = new_label(c);
        int lend = new_label(c);
        gen_expr(c, s->u.ifs.cond, strs);
        bfmt(b, "    test eax, eax\n");
        if (s->u.ifs.nels > 0)
            bfmt(b, "    jz L%d\n", lelse);
        else
            bfmt(b, "    jz L%d\n", lend);
        {
            int i;
            for (i = 0; i < s->u.ifs.nthen; i++)
                gen_stmt(c, s->u.ifs.then[i], retlabel, strs);
        }
        if (s->u.ifs.nels > 0) {
            bfmt(b, "    jmp L%d\n", lend);
            bfmt(b, "L%d:\n", lelse);
            int i;
            for (i = 0; i < s->u.ifs.nels; i++)
                gen_stmt(c, s->u.ifs.els[i], retlabel, strs);
        }
        bfmt(b, "L%d:\n", lend);
        break;
    }
    case S_WHILE: {
        int ltop = new_label(c);
        int lend = new_label(c);
        int save_cont = c->loop_cont;
        int save_end = c->loop_end;
        c->loop_cont = ltop;
        c->loop_end = lend;
        bfmt(b, "L%d:\n", ltop);
        gen_expr(c, s->u.whiles.cond, strs);
        bfmt(b, "    test eax, eax\n    jz L%d\n", lend);
        int i;
        for (i = 0; i < s->u.whiles.n; i++)
            gen_stmt(c, s->u.whiles.body[i], retlabel, strs);
        bfmt(b, "    jmp L%d\n", ltop);
        bfmt(b, "L%d:\n", lend);
        c->loop_cont = save_cont;
        c->loop_end = save_end;
        break;
    }
    case S_FOR: {
        int lstep = new_label(c);
        int lcheck = new_label(c);
        int lend = new_label(c);
        int save_cont = c->loop_cont;
        int save_end = c->loop_end;
        c->loop_cont = lstep;   /* continue → step */
        c->loop_end = lend;

        /* evaluate start, store in loop variable */
        gen_expr(c, s->u.fors.start, strs);
        VarSym *fv = var_of(c, s->u.fors.varname);
        if (!fv) die_at(s->pos, "internal: for loop variable not found");
        gen_store_offs(c, s->u.fors.type, fv->disp);

        /* determine step direction at codegen time */
        Expr *step = s->u.fors.step;
        int step_sign = 1;
        if (step) {
            if (step->kind == E_INT)
                step_sign = step->u.ival >= 0 ? 1 : -1;
            else if (step->kind == E_UN && step->u.un.op == T_MINUS &&
                     step->u.un.a->kind == E_INT)
                step_sign = -1;
            else
                die_at(step->pos, "for loop step must be a constant integer");
        }

        /* jump to first comparison */
        bfmt(b, "    jmp L%d\n", lcheck);

        /* step label */
        bfmt(b, "L%d:\n", lstep);
        if (step) {
            gen_expr(c, step, strs);
            bput(b, "    add dword ");
            emit_mem_ebp(b, fv->disp);
            bput(b, ", eax\n");
        } else {
            bput(b, "    inc dword ");
            emit_mem_ebp(b, fv->disp);
            bput(b, "\n");
        }

        /* comparison: evaluate limit, push, load loop var, pop, compare */
        bfmt(b, "L%d:\n", lcheck);
        gen_expr(c, s->u.fors.limit, strs);
        bput(b, "    push eax\n");
        gen_load_offs(c, s->u.fors.type, fv->disp);
        bput(b, "    pop ecx\n");
        bput(b, "    cmp eax, ecx\n");
        if (step_sign >= 0)
            bfmt(b, "    jge L%d\n", lend);
        else
            bfmt(b, "    jle L%d\n", lend);

        /* body */
        int i;
        for (i = 0; i < s->u.fors.nbody; i++)
            gen_stmt(c, s->u.fors.body[i], retlabel, strs);

        /* back to step */
        bfmt(b, "    jmp L%d\n", lstep);
        bfmt(b, "L%d:\n", lend);
        c->loop_cont = save_cont;
        c->loop_end = save_end;
        break;
    }
    case S_SWITCH: {
        int lend = new_label(c);
        int has_default = 0;
        int i;
        int save_end = c->loop_end;
        c->loop_end = lend;  /* break inside switch exits the switch */

        /* evaluate switch expression, push for comparisons */
        gen_expr(c, s->u.sw.expr, strs);
        bput(b, "    push eax\n");

        /* jump table: compare each case, jump to body or next */
        int *case_labels = xmalloc(sizeof(int) * (size_t)s->u.sw.ncases);
        for (i = 0; i < s->u.sw.ncases; i++) {
            case_labels[i] = new_label(c);
            if (s->u.sw.cases[i].val == -1)
                has_default = 1;
        }

        /* compare and dispatch */
        for (i = 0; i < s->u.sw.ncases; i++) {
            if (s->u.sw.cases[i].val == -1) continue;  /* handle default last */
            bfmt(b, "    pop eax\n");
            bfmt(b, "    push eax\n");
            bfmt(b, "    cmp eax, %lld\n", s->u.sw.cases[i].val);
            bfmt(b, "    je L%d\n", case_labels[i]);
        }

        /* fall-through: no case matched */
        bfmt(b, "    pop eax\n");  /* pop switch expr */
        if (has_default) {
            /* find default label */
            for (i = 0; i < s->u.sw.ncases; i++) {
                if (s->u.sw.cases[i].val == -1) {
                    bfmt(b, "    jmp L%d\n", case_labels[i]);
                    break;
                }
            }
        } else {
            bfmt(b, "    jmp L%d\n", lend);
        }

        /* case bodies */
        for (i = 0; i < s->u.sw.ncases; i++) {
            bfmt(b, "L%d:\n", case_labels[i]);
            /* pop switch expression (already consumed for comparison) */
            if (s->u.sw.cases[i].val != -1)
                bput(b, "    pop eax\n");
            int j;
            for (j = 0; j < s->u.sw.cases[i].nbody; j++)
                gen_stmt(c, s->u.sw.cases[i].body[j], retlabel, strs);
        }

        bfmt(b, "L%d:\n", lend);
        c->loop_end = save_end;
        free(case_labels);
        break;
    }
    case S_BREAK:
        bfmt(b, "    jmp L%d\n", c->loop_end);
        break;
    case S_CONTINUE:
        bfmt(b, "    jmp L%d\n", c->loop_cont);
        break;
    case S_ASSIGN: {
        LVal *lv = &s->u.assign.lv;
        if (lv->nstar > 0) {
            /* *p = v  (or **p = v, ...): value first, then address */
            gen_expr(c, s->u.assign.val, strs);
            bput(b, "    push eax\n");
            VarSym *v = var_of(c, lv->varname);
            int k;
            gen_load_offs(c, v->type, v->disp); /* first pointer value */
            for (k = 1; k < lv->nstar; k++)
                bput(b, "    mov eax, dword [eax]\n");
            bput(b, "    pop ecx\n");
            gen_store_deref(c, lv->type);
        } else if (lv->nsub > 0) {
            /* a[i] = v: value on the stack, compute address, store. */
            gen_expr(c, s->u.assign.val, strs);
            bput(b, "    push eax\n");
            bfmt(b, "    lea eax, ");
            emit_mem_ebp(b, lv->disp);
            bput(b, "\n    push eax\n");
            gen_expr(c, lv->subs[0], strs);
            bput(b, "    mov ecx, eax\n");
            int sz = type_size(lv->type);
            if (sz > 1)
                bfmt(b, "    imul ecx, ecx, %d\n", sz);
            bput(b, "    pop eax\n"
                    "    add eax, ecx\n"
                    "    pop ecx\n");
            gen_store_deref(c, lv->type);
        } else {
            if (type_is_struct(lv->type)) {
                /* struct assignment: s1 = s2
                 * gen_expr loads address of s2 (source) into eax.
                 * We need to compute address of s1 (dest) and memcpy. */
                gen_expr(c, s->u.assign.val, strs);
                bput(b, "    push eax\n"); /* source address */
                if (lv->disp == GLOBAL_DISP)
                    bfmt(b, "    mov eax, swl_g_%s\n", lv->varname);
                else {
                    bfmt(b, "    lea eax, ");
                    emit_mem_ebp(b, lv->disp);
                    bput(b, "\n");
                }
                bput(b, "    pop ecx\n"); /* ecx = source, eax = dest */
                gen_struct_memcpy(b, type_size_of(c->p, lv->type));
            } else {
                gen_expr(c, s->u.assign.val, strs);
                if (lv->disp == GLOBAL_DISP)
                    gen_store_global(c, lv->type, lv->varname);
                else
                    gen_store_offs(c, lv->type, lv->disp);
            }
        }
        break;
    }
    case S_ELSEIF:
        die_at(s->pos, "internal: S_ELSEIF reached codegen (not implemented yet)");
        break;
    case S_EXPR:
        gen_expr(c, s->u.estmt.call, strs);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Functions                                                           */
/* ------------------------------------------------------------------ */

static void gen_fn(Ctx *c, Fn *f, StrTable *strs)
{
    Buf *b = c->out;
    int i;

    c->fn = f;
    c->loop_cont = -1;
    c->loop_end = -1;
    if (strcmp(f->name, "main") == 0)
        bput(b, "swl_main:\n");
    else
        bfmt(b, "swl_fn_%s:\n", f->name);

    bput(b, "    push ebp\n"
            "    mov ebp, esp\n");
    if (f->frame > 0)
        bfmt(b, "    sub esp, %d\n", f->frame);

    if (f->frame > 0)
        bfmt(b, "    cld\n"
                "    mov edi, esp\n"
                "    xor eax, eax\n"
                "    mov ecx, %d\n"
                "    rep stosd\n", f->frame / 4);

    /* Copy struct parameters from hidden pointers to local copies */
    for (i = 0; i < f->nparams; i++) {
        if (type_is_struct(f->params[i].type)) {
            int sz = type_size_of(c->p, f->params[i].type);
            /* source: hidden pointer at [ebp+8+4*i] */
            bfmt(b, "    mov ecx, [ebp + %d]\n", 8 + 4 * i);
            /* dest: local variable (its VarSym has the param name) */
            VarSym *lv = NULL;
            VarSym *v;
            for (v = c->fn->vars; v; v = v->next)
                if (strcmp(v->name, f->params[i].name) == 0) {
                    lv = v;
                    break;
                }
            if (lv) {
                bfmt(b, "    lea eax, ");
                emit_mem_ebp(b, lv->disp);
                bput(b, "\n");
                gen_struct_memcpy(b, sz);
            }
        }
    }

    int retlabel = new_label(c);
    for (i = 0; i < f->nbody; i++)
        gen_stmt(c, f->body[i], retlabel, strs);

    bfmt(b, "L%d:\n"
            "    leave\n"
            "    ret\n", retlabel);
}

/* ------------------------------------------------------------------ */
/* Runtime extern collection                                           */
/* ------------------------------------------------------------------ */

static void collect_externs(Expr *e, char names[32][32], int *n)
{
    int i;
    if (!e)
        return;
    switch (e->kind) {
    case E_CALL:
        if (e->u.call.fni == -1) {
            for (i = 0; i < *n; i++)
                if (strcmp(names[i], e->u.call.name) == 0)
                    break;
            if (i == *n && *n < 32)
                strcpy(names[(*n)++], e->u.call.name);
        }
        for (i = 0; i < e->u.call.nargs; i++)
            collect_externs(e->u.call.args[i], names, n);
        break;
    case E_BIN:
        collect_externs(e->u.bin.a, names, n);
        collect_externs(e->u.bin.b, names, n);
        break;
    case E_UN:
        collect_externs(e->u.un.a, names, n);
        break;
    case E_CAST:
        collect_externs(e->u.cast.a, names, n);
        break;
    case E_SUBSCRIPT:
        collect_externs(e->u.sub.arr, names, n);
        collect_externs(e->u.sub.idx, names, n);
        break;
    default:
        break;
    }
}

static void collect_externs_stmt(Stmt *s, char names[32][32], int *n)
{
    int i;
    if (!s)
        return;
    switch (s->kind) {
    case S_VAR:
        if (s->u.vardecl.init)
            collect_externs(s->u.vardecl.init, names, n);
        break;
    case S_RETURN:
        if (s->u.ret.val)
            collect_externs(s->u.ret.val, names, n);
        break;
    case S_IF:
        collect_externs(s->u.ifs.cond, names, n);
        for (i = 0; i < s->u.ifs.nthen; i++)
            collect_externs_stmt(s->u.ifs.then[i], names, n);
        for (i = 0; i < s->u.ifs.nels; i++)
            collect_externs_stmt(s->u.ifs.els[i], names, n);
        break;
    case S_WHILE:
        collect_externs(s->u.whiles.cond, names, n);
        for (i = 0; i < s->u.whiles.n; i++)
            collect_externs_stmt(s->u.whiles.body[i], names, n);
        break;
    case S_FOR:
        collect_externs(s->u.fors.start, names, n);
        collect_externs(s->u.fors.limit, names, n);
        if (s->u.fors.step)
            collect_externs(s->u.fors.step, names, n);
        for (i = 0; i < s->u.fors.nbody; i++)
            collect_externs_stmt(s->u.fors.body[i], names, n);
        break;
    case S_SWITCH:
        collect_externs(s->u.sw.expr, names, n);
        for (i = 0; i < s->u.sw.ncases; i++) {
            int j;
            for (j = 0; j < s->u.sw.cases[i].nbody; j++)
                collect_externs_stmt(s->u.sw.cases[i].body[j], names, n);
        }
        break;
    case S_ASSIGN:
        collect_externs(s->u.assign.val, names, n);
        for (i = 0; i < s->u.assign.lv.nsub; i++)
            collect_externs(s->u.assign.lv.subs[i], names, n);
        break;
    case S_EXPR:
        collect_externs(s->u.estmt.call, names, n);
        break;
    case S_BREAK:
    case S_CONTINUE:
    case S_ELSEIF:
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */

void codegen(Program *p, const char *outpath)
{
    Buf out = { 0 };
    Ctx c;
    memset(&c, 0, sizeof(c));
    c.p = p;
    c.out = &out;
    c.loop_cont = -1;
    c.loop_end = -1;

    StrTable strs = { 0 };

    bfmt(&out, "; generated by swlc\n"
               "; module %s\n"
               "[bits 32]\n"
               "section .text\n"
               "global swl_main\n", p->name);

    char ext[32][32];
    int next = 0;
    int i, j;
    for (i = 0; i < p->nfuncs; i++)
        for (j = 0; j < p->funcs[i].nbody; j++)
            collect_externs_stmt(p->funcs[i].body[j], ext, &next);
    for (i = 0; i < next; i++)
        bfmt(&out, "extern %s\n", ext[i]);

    for (i = 0; i < p->nfuncs; i++)
        gen_fn(&c, &p->funcs[i], &strs);

    /* emit globals in .data section */
    if (p->nglobals > 0) {
        bput(&out, "\nsection .data\n");
        for (i = 0; i < p->nglobals; i++) {
            GlobalDecl *g = &p->globals[i];
            bfmt(&out, "swl_g_%s:\n", g->name);
            if (type_is_array(g->type) && g->init && g->init->kind == E_STR) {
                int alen = array_len(g->type);
                int blen = g->init->u.str.len;
                int k;
                bput(&out, "    db ");
                for (k = 0; k < alen; k++) {
                    unsigned char bv = k < blen
                        ? (unsigned char)g->init->u.str.bytes[k] : 0;
                    bfmt(&out, "%s0x%02X", k ? "," : "", bv);
                }
                bput(&out, "\n");
            } else if (g->init && g->init->kind == E_INT) {
                bfmt(&out, "    dd %lld\n", g->init->u.ival);
            } else {
                /* zero-init */
                int sz = type_size(g->type);
                if (sz <= 0) sz = 4;
                bfmt(&out, "    times %d db 0\n", sz);
            }
        }
    }
    if (strs.ndefs > 0) {
        if (p->nglobals == 0) bput(&out, "\nsection .data\n");
        for (i = 0; i < strs.ndefs; i++) {
            bfmt(&out, "swl_str_%d:\n    db ", i);
            int k;
            if (strs.defs[i].len == 0) {
                bput(&out, "0\n");
            } else {
                for (k = 0; k < strs.defs[i].len; k++)
                    bfmt(&out, "%s0x%02X",
                         k ? "," : "", (unsigned char)strs.defs[i].bytes[k]);
                bput(&out, ",0\n");
            }
        }
    }

    FILE *fp = fopen(outpath, "wb");
    if (!fp) {
        fprintf(stderr, "swlc: cannot write '%s'\n", outpath);
        exit(1);
    }
    fwrite(out.s, 1, out.len, fp);
    fclose(fp);
    free(out.s);
    for (i = 0; i < strs.ndefs; i++)
        free(strs.defs[i].bytes);
    free(strs.defs);
}
