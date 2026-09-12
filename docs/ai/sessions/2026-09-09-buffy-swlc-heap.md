# Sessão 2026-09-09 — Buffy / swlc V2.11 — heap dinâmico

**Escopo:** `swl_malloc` / `swl_free` / `swl_realloc` / `swl_calloc` / `swl_strdup` no runtime Assembly, registro no sema, exemplo `heap_demo`, 69 testes.

**Motivação:** fechar o conjunto de memória dinâmica; permitir alocação em heap para programas SWL maiores (arrays variáveis, strings duplicadas, structs em heap).

## Runtime (`swl-compiler/src/swlrt.asm`)

Bump allocator via `brk` (syscall 45):

- BSS: `swl_heap_start`, `swl_heap_end`, `swl_heap_ptr`.
- Cada bloco: `[u32 usable_size][dados...]` total arredondado a 4 bytes.
- `swl_malloc(n)` — se `n<=0` retorna NULL; inicializa heap com `brk(0)` na primeira chamada; estende com `brk(new_ptr)` quando necessário; escreve header e retorna `ptr+4`.
- `swl_free(p)` — no-op seguro (mantém bump monotonic). `free(0)` é seguro.
- `swl_realloc(p, n)` — `NULL->malloc(n)`, `n==0->free(p)+NULL`, senão `malloc(n) + memcpy(min(old,n)) + free(old)`. Depende de `swl_memcpy`.
- `swl_calloc(nelem, elsize)` — `mul` nelem*elsize (detecta overflow 32-bit), `malloc(total)`, `memset8(ptr, 0, total)`.
- `swl_strdup(s)` — `strlen(s)+1`, `malloc`, `strcpy`.

Ordem no arquivo: heap vem antes de `swl_time_s`; `swl_calloc`/`swl_strdup` vêm imediatamente antes de `swl_print_hex`; BSS ganhou as três palavras do heap.

## Sema (`swl-compiler/src/sema.c`)

Builtins registrados como `i32`-based (ponteiros são `i32` em x86-32):

```c
{ "swl_malloc",  1, T_I32,  BP_I32,    0,      0 },
{ "swl_free",    1, T_VOID, BP_I32,    0,      0 },
{ "swl_realloc", 2, T_I32,  BP_I32,    BP_I32, 0 },
{ "swl_calloc",  2, T_I32,  BP_I32,    BP_I32, 0 },
{ "swl_strdup",  1, T_I32,  BP_PTR_U8, 0,      0 },
```

Retornos `i32` permitem `var p: i32 = swl_malloc(...)` e `p as *T` para deref; `swl_strdup` aceita string literal `*u8` (decay automático em `sema_expr`).

## Exemplo (`swl-compiler/examples/heap_demo.swl`)

Cobre todos os builtins em um só programa:
- `malloc(8)` + dois `*i32` via `p as *i32` e `(p+4) as *i32`, escreve/lê 123/456.
- `calloc(4,4)` + checa zero-init, escreve 77/88.
- `strdup("hello")` + `strcmp` + `print_str`.
- `malloc(4)` + `realloc(p,8)` preservando 999, escrevendo 1001 em `r2+4`.
- `free(0)` + free de todos os blocos.
- Saída esperada 5 linhas: `malloc ok`, `calloc ok`, `strdup: hello`, `realloc ok`, `heap ok`.

`.out` e `.exit(0)` criados; teste manual `swlc -> nasm -> ld -> ./exe` validado (exit 0, stdout exato).

## Validação

- `make -C swl-compiler clean && make -C swl-compiler && make -C swl-compiler rt` — compila limpo.
- `make -C swl-compiler test` — **69 passed, 0 failed** (34 exemplos + 35 rejeições). Nenhuma regressão.
- Testes manuais adicionais: malloc 8 + ptr arith, calloc+strdup+realloc grow, heap com structs via `sizeof(Node)` e ptr offset — todos exit 0.

## Docs

- `swl-compiler/spec/mvp-subset.md` — novas linhas em M7 para malloc/free/realloc/calloc/strdup + seção V2.11.
- `swl-compiler/README.md` — builtins list atualizada, `heap_demo` na lista, limitação trocada para "bump allocator via brk".
- `docs/ai/PROJECT_STATE.md` — v2.11, 69 verificações, links de sessão, `Strings dinâmicas` marcada ✅.

## Próximos passos

Heap free real (free-list), `swl_memset8` já cobre zero-init do calloc, próximo natural é `swl_printf`/`swl_sprintf` ou GC simples. O bump atual é suficiente para exemplos sem leak massivo.
