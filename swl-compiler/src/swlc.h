/*   swlc.h -- shared definitions for the swlc compiler.
 *
 *   Compiler front/back-end in C, runtime in Assembly (swlrt.asm).
 *   Emits x86-32 NASM assembly.
 */

#ifndef SWLC_H
#define SWLC_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* shared helpers                                                      */
/* ------------------------------------------------------------------ */

typedef struct { int line, col; } SrcPos;

extern const char *g_filename;
void die_at(SrcPos p, const char *fmt, ...);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

/* ------------------------------------------------------------------ */
/* tokens                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    T_EOF,
    T_NL,
    T_IDENT,
    T_INT,
    T_STR,
    T_MODULE, T_FN, T_STRUCT, T_VAR, T_IF, T_ELSE, T_WHILE, T_END,
    T_RETURN, T_AND, T_OR, T_NOT,
    T_CONST, T_AS, T_BREAK, T_CONTINUE, T_SIZEOF,
    T_FOR, T_TO, T_BY, T_GLOBAL,
    T_SWITCH, T_CASE, T_DEFAULT,
    T_LPAREN, T_RPAREN, T_COMMA, T_COLON, T_ARROW, T_DOT, T_ASSIGN,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_AMP, T_LBRACK, T_RBRACK, T_SEMICOLON
} TokKind;

typedef struct {
    TokKind kind;
    SrcPos pos;
    char *text;
    long long ival;
} Token;

Token *lex(const char *src, int *ntok);
const char *tok_name(int kind);

/* ------------------------------------------------------------------ */
/* types                                                               */
/* ------------------------------------------------------------------ */

enum {
    T_UNKNOWN = 0,
    T_I8, T_U8,
    T_I16, T_U16,
    T_I32, T_U32,
    T_ISIZE, T_USIZE,
    T_VOID
};

#define T_STRUCT0 1000   /* struct type ids are T_STRUCT0 + struct index */
#define T_PTR0    3000   /* pointer type ids are T_PTR0 + registry index */
#define T_ARRAY0  5000   /* array type ids are T_ARRAY0 + array index */

int type_size(int t);            /* scalar/pointer/array size; 0 otherwise */
int type_is_signed(int t);
int type_is_int(int t);          /* scalar integer types only */
int type_is_scalar(int t);       /* ints + void */
int type_is_ptr(int t);
int type_is_struct(int t);
int type_is_array(int t);
int type_is_value(int t);        /* ints + pointers (usable as rvalues) */
const char *type_name(int t);    /* scalar names only */
int scalar_type_from_name(const char *s); /* 0 if not a scalar name */

/* ------------------------------------------------------------------ */
/* type registries (shared, cleared per compilation)                  */
/* ------------------------------------------------------------------ */

typedef struct PtrType { int base; } PtrType;
typedef struct ArrayType { int base; int len; } ArrayType;

extern PtrType *ptrs;
extern int nptrs, captrs;
extern ArrayType *arrays;
extern int narrays, cparrays;

int array_base(int t);
int array_len(int t);

/* ------------------------------------------------------------------ */
/* AST                                                                  */
/* ------------------------------------------------------------------ */

struct Program;
struct Fn;
struct Stmt;
struct Expr;
struct LVal;

typedef struct FieldDecl {
    char *name;
    int type;
    int off;
} FieldDecl;

typedef struct StructDecl {
    char *name;
    int type;
    int size;
    FieldDecl *fields;
    int nf;
} StructDecl;

typedef struct ConstDecl {
    char *name;
    int type;
    long long val;
} ConstDecl;

typedef struct GlobalDecl {
    char *name;
    int type;
    struct Expr *init;  /* NULL means zero-initialized */
} GlobalDecl;

typedef struct Param {
    char *name;
    int type;
} Param;

typedef struct VarSym {
    char *name;
    int type;
    int disp;
    struct VarSym *next;
} VarSym;

typedef struct Fn {
    char *name;
    int ret;
    Param *params;
    int nparams, cparams;
    struct Stmt **body;
    int nbody;
    VarSym *vars;
    int frame;
} Fn;

typedef struct LVal {
    char *varname;
    char **chain;
    int nchain;
    int nstar;
    struct Expr **subs;  /* array/pointer subscripts ([a], then apply) */
    int nsub;
    SrcPos pos;
    int type;
    int disp;
    int resolved;
} LVal;

typedef enum {
    S_VAR, S_RETURN, S_IF, S_ELSEIF, S_WHILE, S_FOR, S_BREAK, S_CONTINUE, S_ASSIGN, S_EXPR, S_SWITCH
} StmtKind;

typedef struct SwitchCase {
    long long val;       /* case literal value, or -1 for default */
    struct Stmt **body;
    int nbody;
} SwitchCase;

typedef struct Stmt {
    StmtKind kind;
    SrcPos pos;
    union {
        struct { char *name; int type; struct Expr *init; } vardecl;
        struct { struct Expr *val; } ret;
        struct { struct Expr *cond; struct Stmt **then, **els; int nthen, nels; } ifs;
        struct { struct Expr *cond; struct Stmt **body; int n; } whiles;
        struct { char *varname; int type; struct Expr *start, *limit, *step; struct Stmt **body; int nbody; int has_var; } fors;
        struct { LVal lv; struct Expr *val; } assign;
        struct { struct Expr *call; } estmt;
        struct { struct Expr *cond; struct Stmt **then, **els; int nthen, nels; } elseifs;  /* reused for S_ELSEIF chain */
        struct { struct Expr *expr; SwitchCase *cases; int ncases; } sw;
    } u;
} Stmt;

typedef enum {
    E_INT, E_LVAL, E_CALL, E_BIN, E_UN, E_STR, E_ADDR, E_CONST, E_CAST,
    E_SUBSCRIPT, E_SIZEOF
} ExprKind;

typedef struct Expr {
    ExprKind kind;
    SrcPos pos;
    int type;
    union {
        long long ival;
        LVal lv;
        struct { char *name; struct Expr **args; int nargs; int fni; } call;
        struct { int op; struct Expr *a, *b; int scale; } bin;
        struct { int op; struct Expr *a; } un;
        struct { char *bytes; int len; } str;
        struct { char *name; long long val; } cst;
        struct { struct Expr *a; int to; } cast;
        struct { struct Expr *arr; struct Expr *idx; } sub;
        struct { int ty; } szof;
    } u;
} Expr;

typedef struct Program {
    char *name;
    Fn *funcs;
    int nfuncs, capfuncs;
    ConstDecl *consts;
    int nconsts, capconsts;
    GlobalDecl *globals;
    int nglobals, capglobals;
    StructDecl *structs;
    int nstructs, capstructs;
    PtrType *ptrs;
    int nptrs, captrs;
    ArrayType *arrays;
    int narrays, cparrays;
} Program;

/* helpers that need the full AST types (declared after Program) */
int ptr_type_of(Program *p, int base);
int ptr_base(Program *p, int t);
int type_size_of(Program *p, int t);   /* size incl. struct layout */
void type_disp(Program *p, int t, char *out, size_t n);
StructDecl *struct_by_type(Program *p, int type);
Fn *fn_by_name(Program *p, const char *name);
int array_type_of(Program *p, int base, int len);

void parse_program(Token *toks, int ntok, Program *p);
void sema_check(Program *p);
void codegen(Program *p, const char *outpath);

#endif
