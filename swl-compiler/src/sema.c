/*
 * sema.c -- semantic analysis for the SWL MVP subset.
 *
 * Responsibilities:
 *  - lay out structs (sequential fields, size rounded up to 4),
 *  - resolve names and check types,
 *  - assign every local variable an ebp-relative slot and compute each
 *    function's frame size,
 *  - reject programs that cannot compile (undefined names, type
 *    mismatches, literals that do not fit their context, fall-off-end of
 *    value-returning functions, ...).
 *
 * Locals are zero-initialized by the code generator prologue.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swlc.h"

/* Type registries shared with the rest of the compiler.  They mirror the
 * per-Program tables (one Program is compiled per process), so the
 * array_base()/array_len() helpers and main()'s cleanup both work. */
PtrType *ptrs = NULL;
int nptrs = 0, captrs = 0;
ArrayType *arrays = NULL;
int narrays = 0, cparrays = 0;

/* ------------------------------------------------------------------ */
/* Type helpers                                                        */
/* ------------------------------------------------------------------ */

int type_size(int t)
{
    if (t >= T_ARRAY0)
        return array_len(t) * type_size(array_base(t));
    if (t >= T_PTR0)
        return 4;
    switch (t) {
    case T_I8: case T_U8: return 1;
    case T_I16: case T_U16: return 2;
    case T_I32: case T_U32: case T_ISIZE: case T_USIZE: return 4;
    default: return 0;
    }
}

/* size of a type including struct layout (computed by sema_check) */
int type_size_of(Program *p, int t)
{
    if (type_is_struct(t)) {
        StructDecl *sd = struct_by_type(p, t);
        return sd ? sd->size : 0;
    }
    return type_size(t);
}

int type_is_signed(int t)
{
    switch (t) {
    case T_I8: case T_I16: case T_I32: case T_ISIZE: return 1;
    default: return 0;
    }
}

int type_is_int(int t)
{
    if (t == T_UNKNOWN || t == T_VOID || t >= T_STRUCT0)
        return 0;
    return 1;
}

int type_is_scalar(int t)
{
    return type_is_int(t) || t == T_VOID;
}

int type_is_ptr(int t)
{
    return t >= T_PTR0 && t < T_ARRAY0;
}

int type_is_value(int t)
{
    return type_is_int(t) || type_is_ptr(t);
}

int type_is_struct(int t)
{
    return t >= T_STRUCT0 && t < T_PTR0;
}

int type_is_array(int t)
{
    return t >= T_ARRAY0;
}

int array_base(int t)
{
    int i = t - T_ARRAY0;
    if (i >= 0 && i < narrays)
        return arrays[i].base;
    return T_VOID;
}

int array_len(int t)
{
    int i = t - T_ARRAY0;
    if (i >= 0 && i < narrays)
        return arrays[i].len;
    return 0;
}

const char *type_name(int t)
{
    switch (t) {
    case T_I8: return "i8";
    case T_U8: return "u8";
    case T_I16: return "i16";
    case T_U16: return "u16";
    case T_I32: return "i32";
    case T_U32: return "u32";
    case T_ISIZE: return "isize";
    case T_USIZE: return "usize";
    case T_VOID: return "void";
    default: return "?";
    }
}

int scalar_type_from_name(const char *s)
{
    if (strcmp(s, "i8") == 0) return T_I8;
    if (strcmp(s, "u8") == 0) return T_U8;
    if (strcmp(s, "i16") == 0) return T_I16;
    if (strcmp(s, "u16") == 0) return T_U16;
    if (strcmp(s, "i32") == 0) return T_I32;
    if (strcmp(s, "u32") == 0) return T_U32;
    if (strcmp(s, "isize") == 0) return T_ISIZE;
    if (strcmp(s, "usize") == 0) return T_USIZE;
    return 0;
}

/* ---- type registries ---- */

int ptr_type_of(Program *p, int base)
{
    int i;
    for (i = 0; i < p->nptrs; i++)
        if (p->ptrs[i].base == base)
            return T_PTR0 + i;
    if (p->nptrs == p->captrs) {
        p->captrs = p->captrs ? p->captrs * 2 : 8;
        p->ptrs = xrealloc(p->ptrs, p->captrs * sizeof(PtrType));
    }
    p->ptrs[p->nptrs].base = base;

    for (i = 0; i < nptrs; i++)
        if (ptrs[i].base == base)
            break;
    if (i == nptrs) {
        if (nptrs == captrs) {
            captrs = captrs ? captrs * 2 : 8;
            ptrs = xrealloc(ptrs, captrs * sizeof(PtrType));
        }
        ptrs[nptrs].base = base;
        nptrs++;
    }
    return T_PTR0 + p->nptrs++;
}

int array_type_of(Program *p, int base, int len)
{
    int i;
    for (i = 0; i < p->narrays; i++)
        if (p->arrays[i].base == base && p->arrays[i].len == len)
            return T_ARRAY0 + i;
    if (p->narrays == p->cparrays) {
        p->cparrays = p->cparrays ? p->cparrays * 2 : 8;
        p->arrays = xrealloc(p->arrays, p->cparrays * sizeof(ArrayType));
    }
    p->arrays[p->narrays].base = base;
    p->arrays[p->narrays].len = len;

    for (i = 0; i < narrays; i++)
        if (arrays[i].base == base && arrays[i].len == len)
            break;
    if (i == narrays) {
        if (narrays == cparrays) {
            cparrays = cparrays ? cparrays * 2 : 8;
            arrays = xrealloc(arrays, cparrays * sizeof(ArrayType));
        }
        arrays[narrays].base = base;
        arrays[narrays].len = len;
        narrays++;
    }
    return T_ARRAY0 + p->narrays++;
}

int ptr_base(Program *p, int t)
{
    int idx = t - T_PTR0;
    if (idx >= 0 && idx < p->nptrs)
        return p->ptrs[idx].base;
    return T_VOID;
}

void type_disp(Program *p, int t, char *out, size_t n)
{
    if (t >= T_ARRAY0) {
        char base[64];
        type_disp(p, array_base(t), base, sizeof(base));
        snprintf(out, n, "[%s, %d]", base, array_len(t));
    } else if (t >= T_PTR0) {
        char base[64];
        type_disp(p, ptr_base(p, t), base, sizeof(base));
        snprintf(out, n, "*%s", base);
    } else if (t >= T_STRUCT0) {
        StructDecl *s = struct_by_type(p, t);
        snprintf(out, n, "%s", s ? s->name : "?struct");
    } else {
        snprintf(out, n, "%s", type_name(t));
    }
}

StructDecl *struct_by_type(Program *p, int type)
{
    int idx = type - T_STRUCT0;
    if (idx >= 0 && idx < p->nstructs)
        return &p->structs[idx];
    return NULL;
}

Fn *fn_by_name(Program *p, const char *name)
{
    int i;
    for (i = 0; i < p->nfuncs; i++)
        if (strcmp(p->funcs[i].name, name) == 0)
            return &p->funcs[i];
    return NULL;
}

static ConstDecl *const_by_name(Program *p, const char *name)
{
    int i;
    for (i = 0; i < p->nconsts; i++)
        if (strcmp(p->consts[i].name, name) == 0)
            return &p->consts[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Semantic context                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    Program *p;
    Fn *fn;
    VarSym *vars;    /* param + local symbols of the current function */
    int frame;       /* bytes of local frame allocated so far */
    int loop_depth;  /* >0 inside a while body */
} Ctx;

/* printable type name (rotating buffer so two may appear in one message) */
static char *tyname(Ctx *c, int t)
{
    static char buf[4][64];
    static int idx;
    char *out = buf[idx & 3];
    idx++;
    type_disp(c->p, t, out, sizeof(buf[0]));
    return out;
}

static VarSym *find_var(Ctx *c, const char *name)
{
    VarSym *v;
    for (v = c->vars; v; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

static int literal_fits(int t, long long v)
{
    if (type_is_ptr(t))
        return v == 0; /* only NULL is a pointer literal */
    switch (t) {
    case T_I8:  return v >= -128 && v <= 127;
    case T_U8:  return v >= 0 && v <= 255;
    case T_I16: return v >= -32768 && v <= 32767;
    case T_U16: return v >= 0 && v <= 65535;
    case T_I32:
    case T_ISIZE:
        return v >= -2147483648LL && v <= 2147483647LL;
    case T_U32:
    case T_USIZE:
        return v >= 0 && v <= 4294967295LL;
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Expression checking                                                 */
/* ------------------------------------------------------------------ */

static void sema_expr(Ctx *c, Expr *e, int want);
static void sema_stmt(Ctx *c, Stmt *s);

static void require_value(Ctx *c, Expr *e, const char *what)
{
    if (type_is_array(e->type))
        die_at(e->pos, "%s must be an integer or pointer expression (got "
               "an array; index it with '[...]' or pass it where a "
               "pointer is expected)", what);
    if (!type_is_value(e->type))
        die_at(e->pos, "%s must be an integer or pointer expression "
               "(got %s)", what, tyname(c, e->type));
}

static int global_index(Program *p, const char *name)
{
    int i;
    for (i = 0; i < p->nglobals; i++)
        if (strcmp(p->globals[i].name, name) == 0)
            return i;
    return -1;
}

#define GLOBAL_DISP (-999999999)

static void resolve_var_chain(Ctx *c, LVal *lv)
{
    VarSym *v = find_var(c, lv->varname);
    if (!v) {
        /* check program globals */
        int gi = global_index(c->p, lv->varname);
        if (gi >= 0 && lv->nchain == 0) {
            lv->type = c->p->globals[gi].type;
            lv->disp = GLOBAL_DISP;
            lv->resolved = 1;
        }
        return; /* caller decides between const fallback and error */
    }

    int type = v->type;
    int disp = v->disp;
    int i;

    for (i = 0; i < lv->nchain; i++) {
        if (!type_is_struct(type))
            die_at(lv->pos, "cannot access field '%s' of non-struct type %s",
                   lv->chain[i], tyname(c, type));
        StructDecl *s = struct_by_type(c->p, type);
        int f;
        for (f = 0; f < s->nf; f++)
            if (strcmp(s->fields[f].name, lv->chain[i]) == 0)
                break;
        if (f == s->nf)
            die_at(lv->pos, "struct '%s' has no field named '%s'",
                   s->name, lv->chain[i]);
        disp += s->fields[f].off;
        type = s->fields[f].type;
    }

    if (type_is_struct(type))
        die_at(lv->pos,
               "cannot use a whole struct as a value in the MVP "
               "(access a field instead)");

    lv->type = type;
    lv->disp = disp;
    lv->resolved = 1;
}

/* Resolve an lvalue used as a *write target with deref stars*. */
static void resolve_deref_lval(Ctx *c, LVal *lv)
{
    VarSym *v = find_var(c, lv->varname);
    if (!v)
        die_at(lv->pos, "undefined variable '%s'", lv->varname);
    int type = v->type;
    int k;
    for (k = 0; k < lv->nstar; k++) {
        if (!type_is_ptr(type))
            die_at(lv->pos, "cannot dereference %s (type %s)",
                   k == 0 ? lv->varname : "a pointer",
                   tyname(c, type));
        type = ptr_base(c->p, type);
    }
    if (type_is_struct(type))
        die_at(lv->pos, "cannot store through a pointer to struct %s",
               tyname(c, type));
    if (!type_is_value(type))
        die_at(lv->pos, "invalid write target type %s", tyname(c, type));
    lv->type = type;
    lv->disp = 0;
    lv->resolved = 1;
}

/* Builtin runtime functions callable from SWL (defined in swlrt.asm). */
typedef struct { const char *name; int nargs; int ret; int p1, p2, p3; } Builtin;

enum { BP_I32 = 0, BP_PTR_U8 = 1, BP_U32 = 2 };

static const Builtin builtins[] = {
    { "swl_print_i32", 1, T_VOID, BP_I32,    0,        0 },
    { "swl_print_u32", 1, T_VOID, BP_U32,    0,        0 },
    { "swl_putchar",   1, T_VOID, BP_I32,    0,        0 },
    { "swl_print_char",1, T_VOID, BP_I32,    0,        0 },
    { "swl_exit",      1, T_VOID, BP_I32,    0,        0 },
    { "swl_print_str", 1, T_VOID, BP_PTR_U8, 0,        0 },
    { "swl_strlen",    1, T_I32,  BP_PTR_U8, 0,        0 },
    { "swl_strcmp",    2, T_I32,  BP_PTR_U8, BP_PTR_U8, 0 },
    { "swl_memset8",   3, T_VOID, BP_PTR_U8, BP_I32,  BP_I32 },
    { "swl_time_s",    0, T_I32,  0,         0,        0 },
    { "swl_rand",      0, T_I32,  0,         0,        0 },
    { "swl_srand",     1, T_VOID, BP_I32,    0,        0 },
    { "swl_print_hex", 1, T_VOID, BP_I32,    0,        0 },
    { NULL, 0, 0, 0, 0, 0 }
};

static int builtin_index(const char *name)
{
    int i;
    for (i = 0; builtins[i].name; i++)
        if (strcmp(builtins[i].name, name) == 0)
            return i;
    return -1;
}

/* true if the subtree is nothing but literals folded with arithmetic
 * (it has no type until a context gives it one) */
static int is_pure_lit(Expr *e)
{
    if (e->kind == E_INT)
        return 1;
    if (e->kind == E_UN && e->u.un.op == T_MINUS)
        return is_pure_lit(e->u.un.a);
    if (e->kind == E_BIN) {
        int op = e->u.bin.op;
        if (op == T_AND || op == T_OR)
            return 0;
        if (op != T_PLUS && op != T_MINUS && op != T_STAR &&
            op != T_SLASH && op != T_PERCENT)
            return 0; /* comparisons yield i32 and are never literal */
        return is_pure_lit(e->u.bin.a) && is_pure_lit(e->u.bin.b);
    }
    return 0;
}

static void sema_bin(Ctx *c, Expr *e, int want)
{
    Expr *a = e->u.bin.a, *b = e->u.bin.b;
    int op = e->u.bin.op;

    if (op == T_AND || op == T_OR) {
        sema_expr(c, a, T_UNKNOWN);
        sema_expr(c, b, T_UNKNOWN);
        require_value(c, a, "operand of 'and'/'or'");
        require_value(c, b, "operand of 'and'/'or'");
        e->type = T_I32;
        return;
    }

    /* arithmetic / comparison: operands must end with the same type.  An
     * untyped operand is always a pure literal expression; it adopts the
     * type of its typed sibling (or of the outer context when the whole
     * operation is literal, defaulting to i32).  Typed subtrees are never
     * forced to a parent's type. */
    int arith = op == T_PLUS || op == T_MINUS || op == T_STAR ||
                op == T_SLASH || op == T_PERCENT;
    int eqop = op == T_EQ || op == T_NE;
    int ptr_op = op == T_PLUS || op == T_MINUS;

    if (is_pure_lit(a) && is_pure_lit(b)) {
        int tgt = type_is_int(want) ? want : T_I32;
        sema_expr(c, a, tgt);
        sema_expr(c, b, tgt);
    } else if (is_pure_lit(a)) {
        sema_expr(c, b, T_UNKNOWN);
        if (type_is_value(b->type)) {
            if (arith && type_is_ptr(b->type) && !ptr_op)
                die_at(e->pos, "only '+' and '-' are defined on pointers");
            /* ptr +- literal: the literal stays an integer and is
             * scaled by the base size (pointer arithmetic) */
            if (arith && ptr_op && type_is_ptr(b->type))
                sema_expr(c, a, T_I32);
            else
                sema_expr(c, a, b->type);
        }
    } else if (is_pure_lit(b)) {
        sema_expr(c, a, T_UNKNOWN);
        if (type_is_value(a->type)) {
            if (arith && type_is_ptr(a->type) && !ptr_op)
                die_at(e->pos, "only '+' and '-' are defined on pointers");
            if (arith && ptr_op && type_is_ptr(a->type))
                sema_expr(c, b, T_I32);
            else
                sema_expr(c, b, a->type);
        }
    } else {
        sema_expr(c, a, T_UNKNOWN);
        sema_expr(c, b, T_UNKNOWN);
    }

    /* array decay in arithmetic: buf + i / i + buf / buf - i walk the
     * buffer through a pointer to its element type */
    if (arith && ptr_op &&
        (type_is_array(a->type) || type_is_array(b->type))) {
        Expr *av = type_is_array(a->type) ? a : b;
        Expr *iv = type_is_array(a->type) ? b : a;
        if (type_is_array(iv->type))
            die_at(e->pos, "cannot use two arrays in one expression "
                   "(decay one to a pointer first)");
        if (av->kind != E_LVAL)
            die_at(e->pos, "cannot decay this array expression to a "
                   "pointer in the MVP (use a pointer variable)");
        if (iv->type == T_UNKNOWN)
            sema_expr(c, iv, T_I32);
        if (op == T_MINUS && iv == a)
            die_at(e->pos, "cannot subtract an array from an integer");
        if (!type_is_int(iv->type))
            die_at(e->pos, "type mismatch: cannot apply %s to %s and %s",
                   tok_name(op), tyname(c, a->type), tyname(c, b->type));
        av->kind = E_ADDR;
        av->type = ptr_type_of(c->p, array_base(av->type));
    }

    int both_int = type_is_int(a->type) && type_is_int(b->type);
    int both_val = type_is_value(a->type) && type_is_value(b->type);

    /* pointer arithmetic: ptr + int, int + ptr, ptr - int */
    if (arith && ptr_op &&
        ((type_is_ptr(a->type) && type_is_int(b->type)) ||
         (type_is_int(a->type) && type_is_ptr(b->type)))) {
        if (op == T_MINUS && type_is_ptr(b->type))
            die_at(e->pos, "cannot subtract a pointer from an integer");
        int pv = type_is_ptr(a->type) ? a->type : b->type;
        int base = ptr_base(c->p, pv);
        int sz = type_size(base);
        if (sz <= 0)
            die_at(e->pos, "arithmetic on %s pointers is not supported",
                   tyname(c, pv));
        e->type = pv;
        e->u.bin.scale = sz;
        return;
    }

    if (!both_val)
        die_at(e->pos, "operands of %s must be integers or pointers "
               "(got %s and %s)",
               tok_name(op), tyname(c, a->type), tyname(c, b->type));

    if (arith && type_is_ptr(a->type) && type_is_ptr(b->type)) {
        if (op == T_MINUS)
            die_at(e->pos, "pointer difference is not supported in "
                   "this version");
        die_at(e->pos, "cannot add two pointers");
    }

    if (a->type != b->type)
        die_at(e->pos, "type mismatch: cannot apply %s to %s and %s "
               "(no implicit conversions in the MVP)",
               tok_name(op), tyname(c, a->type), tyname(c, b->type));

    if (eqop) {
        e->type = T_I32;
        return;
    }
    if (!both_int) {
        die_at(e->pos, "cannot apply %s to pointers (only == and !=)",
               tok_name(op));
    }
    e->type = arith ? a->type : T_I32;
}

static void sema_expr(Ctx *c, Expr *e, int want)
{
    switch (e->kind) {
    case E_INT: {
        int t = want != T_UNKNOWN ? want : T_I32;
        if (!type_is_value(t))
            die_at(e->pos, "integer literal cannot be used where %s is "
                   "expected", tyname(c, t));
        if (!literal_fits(t, e->u.ival))
            die_at(e->pos, "integer literal %lld does not fit %s",
                   e->u.ival, tyname(c, t));
        e->type = t;
        break;
    }
    case E_LVAL: {
        /* variable read (possibly a const reference if not found) */
        VarSym *v = find_var(c, e->u.lv.varname);
        if (!v) {
            ConstDecl *k = e->u.lv.nchain == 0 && e->u.lv.nstar == 0 &&
                                   e->u.lv.nsub == 0
                               ? const_by_name(c->p, e->u.lv.varname)
                               : NULL;
            if (k) {
                e->kind = E_CONST;
                e->u.cst.name = k->name;
                e->u.cst.val = k->val;
                e->type = k->type;
                break;
            }
        }
        resolve_var_chain(c, &e->u.lv);
        if (!e->u.lv.resolved)
            die_at(e->pos, "undefined variable '%s'", e->u.lv.varname);
        int t = e->u.lv.type;
        if (type_is_array(t)) {
            /* array -> pointer decay when a pointer to the element type
             * is expected (e.g. passing a local string buffer to
             * swl_strlen / swl_print_str). */
            if (want != T_UNKNOWN && type_is_ptr(want) &&
                ptr_base(c->p, want) == array_base(t)) {
                e->kind = E_ADDR;
                e->type = want;
                break;
            }
            /* Otherwise the array keeps its type; callers decide:
             * indexing (E_SUBSCRIPT) is fine, and require_value()/the
             * want-mismatch check reject it as a scalar. */
        }
        e->type = t;
        break;
    }
    case E_ADDR: {
        resolve_var_chain(c, &e->u.lv);
        if (!e->u.lv.resolved)
            die_at(e->pos, "undefined variable '%s'", e->u.lv.varname);
        if (type_is_struct(e->u.lv.type))
            die_at(e->pos,
                   "cannot take the address of a whole struct in this "
                   "version (address a field instead)");
        e->type = ptr_type_of(c->p, e->u.lv.type);
        break;
    }
    case E_CALL: {
        int bi = builtin_index(e->u.call.name);
        int i;
        if (bi >= 0) {
            if (e->u.call.nargs != builtins[bi].nargs)
                die_at(e->pos, "builtin '%s' expects %d argument(s), got %d",
                       e->u.call.name, builtins[bi].nargs,
                       e->u.call.nargs);
            for (i = 0; i < e->u.call.nargs; i++) {
                int pk = i == 0 ? builtins[bi].p1
                       : i == 1 ? builtins[bi].p2
                       : builtins[bi].p3;
                int want_t = pk == BP_PTR_U8
                                 ? ptr_type_of(c->p, T_U8)
                                 : pk == BP_U32
                                 ? T_U32
                                 : T_I32;
                sema_expr(c, e->u.call.args[i], want_t);
            }
            e->type = builtins[bi].ret;
            e->u.call.fni = -1;
            break;
        }
        Fn *f = fn_by_name(c->p, e->u.call.name);
        if (!f)
            die_at(e->pos, "call to unknown function '%s'",
                   e->u.call.name);
        if (f->nparams != e->u.call.nargs)
            die_at(e->pos, "function '%s' expects %d argument(s), got %d",
                   f->name, f->nparams, e->u.call.nargs);
        for (i = 0; i < f->nparams; i++)
            sema_expr(c, e->u.call.args[i], f->params[i].type);
        e->u.call.fni = (int)(f - c->p->funcs);
        e->type = f->ret;
        break;
    }
    case E_STR:
        e->type = ptr_type_of(c->p, T_U8);
        break;
    case E_SIZEOF: {
        int sz = type_size_of(c->p, e->u.szof.ty);
        if (sz <= 0)
            die_at(e->pos, "cannot take sizeof of type '%s'",
                   tyname(c, e->u.szof.ty));
        e->type = T_I32;
        break;
    }
    case E_BIN:
        sema_bin(c, e, want);
        break;
    case E_UN:
        switch (e->u.un.op) {
        case T_MINUS: {
            sema_expr(c, e->u.un.a, T_UNKNOWN);
            if (!type_is_int(e->u.un.a->type))
                die_at(e->pos, "operand of unary - must be an integer "
                       "(got %s)", tyname(c, e->u.un.a->type));
            e->type = e->u.un.a->type;
            break;
        }
        case T_NOT:
            sema_expr(c, e->u.un.a, T_UNKNOWN);
            require_value(c, e->u.un.a, "operand of 'not'");
            e->type = T_I32;
            break;
        case T_STAR: { /* dereference */
            sema_expr(c, e->u.un.a, T_UNKNOWN);
            if (!type_is_ptr(e->u.un.a->type))
                die_at(e->pos, "cannot dereference a non-pointer "
                       "(got %s)", tyname(c, e->u.un.a->type));
            int base = ptr_base(c->p, e->u.un.a->type);
            if (type_is_struct(base))
                die_at(e->pos, "cannot dereference a pointer to struct %s "
                       "in this version (dereference pointers to scalars)",
                       tyname(c, base));
            e->type = base;
            break;
        }
        }
        break;
    case E_CONST:
        /* produced by morphing E_LVAL during resolution (already typed) */
        break;
    case E_CAST: {
        sema_expr(c, e->u.cast.a, T_UNKNOWN);
        int from = e->u.cast.a->type;
        int to = e->u.cast.to;
        if (type_is_int(from) && type_is_int(to)) {
            ; /* width/sign conversion */
        } else if ((type_is_ptr(from) && type_is_int(to)) ||
                   (type_is_int(from) && type_is_ptr(to)) ||
                   (type_is_ptr(from) && type_is_ptr(to))) {
            ; /* reinterpretation; same 32-bit representation */
        } else {
            die_at(e->pos, "cannot convert %s to %s with 'as' (only "
                   "integers and pointers are convertible)",
                   tyname(c, from), tyname(c, to));
        }
        e->type = to;
        break;
    }
    case E_SUBSCRIPT: {
        sema_expr(c, e->u.sub.arr, T_UNKNOWN);
        sema_expr(c, e->u.sub.idx, T_UNKNOWN);
        require_value(c, e->u.sub.idx, "array index");
        if (!type_is_int(e->u.sub.idx->type))
            die_at(e->pos, "array index must be an integer (got %s)",
                   tyname(c, e->u.sub.idx->type));
        int arr = e->u.sub.arr->type;
        int base = T_VOID;
        if (type_is_array(arr)) {
            base = array_base(arr);
        } else if (type_is_ptr(arr)) {
            base = ptr_base(c->p, arr);
        } else {
            die_at(e->pos, "subscript requires an array or pointer "
                   "(got %s)", tyname(c, arr));
        }
        if (type_is_struct(base))
            die_at(e->pos, "cannot index arrays of struct %s in this "
                   "version", tyname(c, base));
        e->type = base;
        break;
    }
    }

    if (e->type == T_VOID && want != T_UNKNOWN)
        die_at(e->pos, "this expression returns nothing (void) and cannot "
               "be used as a value");
    if (e->type != T_VOID && want != T_UNKNOWN && e->type != want)
        die_at(e->pos, "type mismatch: expected %s, got %s",
               tyname(c, want), tyname(c, e->type));
}

/* ------------------------------------------------------------------ */
/* Statement checking + local layout                                   */
/* ------------------------------------------------------------------ */

static void declare_local(Ctx *c, SrcPos pos, char *name, int type)
{
    if (find_var(c, name))
        die_at(pos, "duplicate variable '%s' in function '%s'",
               name, c->fn->name);
    VarSym *v = xmalloc(sizeof(VarSym));
    v->name = name;
    v->type = type;
    int slot;
    if (type_is_struct(type))
        slot = (struct_by_type(c->p, type)->size + 3) & ~3;
    else if (type_is_array(type))
        slot = (type_size(type) + 3) & ~3;
    else
        slot = 4;
    c->frame += slot;
    v->disp = -c->frame;
    v->next = c->vars;
    c->vars = v;
}

static void sema_stmt(Ctx *c, Stmt *s)
{
    int i;
    switch (s->kind) {
    case S_VAR: {
        int t = s->u.vardecl.type;
        if (!type_is_value(t) && !type_is_struct(t) && !type_is_array(t))
            die_at(s->pos, "cannot declare a variable of type %s",
                   tyname(c, t));
        declare_local(c, s->pos, s->u.vardecl.name, t);
        if (s->u.vardecl.init) {
            if (type_is_struct(t))
                die_at(s->pos, "struct variables cannot be initialized in "
                       "the MVP");
            if (type_is_array(t)) {
                /* The only array initializer in the MVP is a string
                 * literal, and only for [u8, N]: the code generator
                 * stores the bytes element by element. */
                Expr *init = s->u.vardecl.init;
                if (init->kind != E_STR)
                    die_at(s->pos,
                           "array initialization requires a string literal "
                           "(use \"...\" syntax)");
                if (array_base(t) != T_U8)
                    die_at(s->pos,
                           "only [u8, N] arrays can be initialized from a "
                           "string literal in the MVP");
                if (init->u.str.len > array_len(t))
                    die_at(s->pos,
                           "string literal too long for array %s "
                           "(need %d bytes, have %d)",
                           tyname(c, t), array_len(t), init->u.str.len);
            } else {
                sema_expr(c, s->u.vardecl.init, t);
            }
        }
        break;
    }
    case S_RETURN: {
        if (c->fn->ret == T_VOID) {
            if (s->u.ret.val)
                die_at(s->pos, "'return' with a value in void function "
                       "'%s'", c->fn->name);
        } else {
            if (!s->u.ret.val)
                die_at(s->pos, "'return' without a value in function "
                       "'%s' (returns %s)",
                       c->fn->name, tyname(c, c->fn->ret));
            sema_expr(c, s->u.ret.val, c->fn->ret);
        }
        break;
    }
    case S_IF:
        sema_expr(c, s->u.ifs.cond, T_UNKNOWN);
        require_value(c, s->u.ifs.cond, "if condition");
        for (i = 0; i < s->u.ifs.nthen; i++)
            sema_stmt(c, s->u.ifs.then[i]);
        for (i = 0; i < s->u.ifs.nels; i++)
            sema_stmt(c, s->u.ifs.els[i]);
        break;
    case S_WHILE:
        sema_expr(c, s->u.whiles.cond, T_UNKNOWN);
        require_value(c, s->u.whiles.cond, "while condition");
        c->loop_depth++;
        for (i = 0; i < s->u.whiles.n; i++)
            sema_stmt(c, s->u.whiles.body[i]);
        c->loop_depth--;
        break;
    case S_FOR: {
        sema_expr(c, s->u.fors.start, T_UNKNOWN);
        sema_expr(c, s->u.fors.limit, T_UNKNOWN);
        if (s->u.fors.step)
            sema_expr(c, s->u.fors.step, T_UNKNOWN);
        /* declare loop variable as a function-local (stays in fn->vars
         * so codegen can find it; occupies one stack slot for the
         * entire function — negligible cost). */
        declare_local(c, s->pos, s->u.fors.varname, s->u.fors.type);
        c->loop_depth++;
        for (i = 0; i < s->u.fors.nbody; i++)
            sema_stmt(c, s->u.fors.body[i]);
        c->loop_depth--;
        break;
    }
    case S_BREAK:
        if (c->loop_depth == 0)
            die_at(s->pos, "'break' outside a loop");
        break;
    case S_CONTINUE:
        if (c->loop_depth == 0)
            die_at(s->pos, "'continue' outside a loop");
        break;
    case S_ASSIGN: {
        LVal *lv = &s->u.assign.lv;
        if (lv->nstar > 0) {
            if (lv->nsub > 0)
                die_at(s->pos, "cannot combine '*' and '[...]' on the "
                       "left side in the MVP");
            resolve_deref_lval(c, lv);
        } else {
            resolve_var_chain(c, lv);
            if (!lv->resolved)
                die_at(s->pos, "undefined variable '%s'", lv->varname);
            if (lv->nsub == 0 && type_is_array(lv->type))
                die_at(s->pos, "cannot assign to an entire array in the "
                       "MVP (assign an element: 'name[i] = ...')");
            if (lv->nsub > 0) {
                if (lv->nsub > 1)
                    die_at(s->pos, "only a single '[...]' subscript is "
                           "supported on the left side in the MVP");
                if (!type_is_array(lv->type)) {
                    if (type_is_ptr(lv->type))
                        die_at(s->pos, "pointer subscripts on the left "
                               "side are not supported in the MVP "
                               "(index arrays with '[...]')");
                    die_at(s->pos,
                           "subscript target must be an array "
                           "(got %s)", tyname(c, lv->type));
                }
                int t = array_base(lv->type);
                if (!type_is_value(t))
                    die_at(s->pos,
                           "array element must be an integer or pointer "
                           "(cannot assign whole sub-arrays in the MVP)");
                lv->type = t;
            }
        }
        sema_expr(c, s->u.assign.val, lv->type);
        break;
    }
    case S_EXPR:
        sema_expr(c, s->u.estmt.call, T_UNKNOWN);
        if (s->u.estmt.call->kind != E_CALL)
            die_at(s->pos, "internal error: non-call expression statement");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Program entry                                                       */
/* ------------------------------------------------------------------ */

void sema_check(Program *p)
{
    int i, f;

    /* struct layout: sequential fields, no padding inside; total size
     * rounded up to a multiple of 4 so slots stay aligned.  Structs can
     * only reference structs declared earlier in the file. */
    for (i = 0; i < p->nstructs; i++) {
        StructDecl *s = &p->structs[i];
        int off = 0, k;
        for (k = 0; k < s->nf; k++) {
            int t = s->fields[k].type;
            if (type_is_struct(t)) {
                StructDecl *inner = struct_by_type(p, t);
                if (!inner || inner >= s)
                    die_at((SrcPos){ 0, 0 },
                           "struct '%s': cannot reference struct '%s' here "
                           "(only earlier structs can be nested in the MVP)",
                           s->name, s->fields[k].name);
            }
            s->fields[k].off = off;
            if (type_is_struct(t))
                off += struct_by_type(p, t)->size;
            else
                off += type_size(t);
        }
        s->size = (off + 3) & ~3;
    }

    /* constants: value must fit the declared integer type */
    for (i = 0; i < p->nconsts; i++) {
        ConstDecl *k = &p->consts[i];
        if (!literal_fits(k->type, k->val))
            die_at((SrcPos){ 0, 0 },
                   "constant '%s': value %lld does not fit %s",
                   k->name, k->val, type_name(k->type));
    }

    /* globals: type-check init expressions */
    for (i = 0; i < p->nglobals; i++) {
        GlobalDecl *g = &p->globals[i];
        if (g->init) {
            /* for the MVP, init must be an integer literal, a string
             * literal (for [u8, N] globals), or a constant name. */
            if (type_is_array(g->type) && g->init->kind == E_STR) {
                /* string init for array -- OK */
            } else if (type_is_int(g->type) && g->init->kind == E_INT) {
                /* integer literal -- OK */
            } else if (g->init->kind == E_LVAL) {
                /* constant reference */
            } else {
                die_at(g->init->pos,
                       "global '%s': initializer must be a literal or "
                       "constant", g->name);
            }
        }
    }

    /* functions: parameters are pre-declared at [ebp+8..], locals are
     * laid out below ebp as their declarations are visited. */
    for (i = 0; i < p->nfuncs; i++) {
        Fn *fn = &p->funcs[i];
        Ctx c;
        memset(&c, 0, sizeof(c));
        c.p = p;
        c.fn = fn;

        int pi;
        for (pi = fn->nparams - 1; pi >= 0; pi--) {
            Param *par = &fn->params[pi];
            VarSym *v = xmalloc(sizeof(VarSym));
            v->name = par->name;
            v->type = par->type;
            v->disp = 8 + 4 * pi;
            v->next = c.vars;
            c.vars = v;
        }

        for (f = 0; f < fn->nbody; f++)
            sema_stmt(&c, fn->body[f]);

        fn->frame = c.frame;
        fn->vars = c.vars;

        /* value-returning functions must end with `return value` so they
         * cannot fall off the end (MVP rule, keeps codegen simple). */
        if (fn->ret != T_VOID) {
            if (fn->nbody == 0 ||
                fn->body[fn->nbody - 1]->kind != S_RETURN ||
                !fn->body[fn->nbody - 1]->u.ret.val) {
                Stmt *last = fn->nbody ? fn->body[fn->nbody - 1] : NULL;
                die_at(last ? last->pos : (SrcPos){ 0, 0 },
                       "function '%s' returns %s but does not end with "
                       "'return <value>'", fn->name,
                       tyname(&c, fn->ret));
            }
        }
    }

    /* the module must define an entry point */
    Fn *main = fn_by_name(p, "main");
    if (!main)
        die_at((SrcPos){ 0, 0 },
               "module '%s' must define 'fn main() -> i32'", p->name);
    if (main->nparams != 0)
        die_at((SrcPos){ 0, 0 },
               "'main' cannot take parameters in the MVP");
    if (!type_is_int(main->ret))
        die_at((SrcPos){ 0, 0 },
               "'main' must return an integer type (the exit code)");
}
