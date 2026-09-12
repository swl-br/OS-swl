# Sessão — V2.15: IPC (pipe/dup/dup2) + fix subscript index

**IA:** Buffy (Codebuff)
**Data:** 2026-09-09
**Responsável:** Implementação de IPC e fix de bug no sema

## Objetivo

Adicionar suporte a IPC via `swl_pipe`, `swl_dup` e `swl_dup2`, e corrigir
um bug crítico no sema onde expressões de índice em subscript de assignment
(`a[i] = v`) não eram resolvidas, causando codegen incorreto.

## Alterações

### Arquivos modificados:
- `swl-compiler/src/swlrt.asm` — adicionadas `swl_pipe`, `swl_dup`, `swl_dup2`
- `swl-compiler/src/sema.c` — registrados builtins + fix: `sema_expr` em
  índices de subscript no `S_ASSIGN`; adicionado `BP_PTR_I32`
- `swl-compiler/examples/pipe_demo.swl` — novo exemplo IPC
- `swl-compiler/examples/pipe_demo.out` — fixture stdout
- `swl-compiler/examples/pipe_demo.exit` — fixture exit code
- `swl-compiler/README.md` — documentação atualizada
- `swl-compiler/spec/mvp-subset.md` — seção V2.15 adicionada
- `docs/ai/PROJECT_STATE.md` — v2.15, 73 verificações

### Arquivos criados:
- `docs/ai/sessions/2026-09-09-buffy-swlc-pipe.md` — esta sessão

## Implementação

### Runtime (swlrt.asm)

Três wrappers diretos para syscalls Linux i386:

```
swl_pipe(fds: *i32) -> i32
  ; ebx = fds, eax = 42 (sys_pipe)
  ; escreve dois int em fds[0] e fds[1]

swl_dup(oldfd: i32) -> i32
  ; ebx = oldfd, eax = 41 (sys_dup)

swl_dup2(oldfd: i32, newfd: i32) -> i32
  ; ebx = oldfd, ecx = newfd, eax = 63 (sys_dup2)
```

### Sema (sema.c)

- Novo tipo de parâmetro `BP_PTR_I32 = 3` para builtins que aceitam `*i32`
  (diferente de `BP_PTR_U8` que aceita `*u8`).
- `swl_pipe(fds: *i32) -> i32` registrado com `BP_PTR_I32`
- `swl_dup(oldfd: i32) -> i32` e `swl_dup2(oldfd: i32, newfd: i32) -> i32`
  registrados com `BP_I32`.
- **Fix crítico**: no `S_ASSIGN` com subscript (`a[i] = v`), o sema agora
  chama `sema_expr(c, lv->subs[i], T_UNKNOWN)` para cada índice. Antes,
  as expressões de índice não eram resolvidas — o campo `disp` ficava 0
  (memset zero do `new_expr`), e o codegen gerava `mov eax, dword [ebp+0]`
  em vez de `mov eax, dword [ebp+disp]`.

### Exemplo pipe_demo.swl

Três padrões de IPC validados:
1. **Pipe básico**: write/read no mesmo processo
2. **Fork + pipe**: child escreve "child\n", parent lê e compara

Saída verificada byte a byte: 3 linhas, exit 0.

## Decisões

- `swl_dup` e `swl_dup2` foram registrados no sema mesmo sem exemplo
  dedicado — são necessários para redirecionamento de I/O em programas
  reais (ex.: shell, pipelines). O codegen funciona corretamente; o
  exemplo `pipe_demo` se concentra nos padrões mais comuns (pipe + fork).
- O fix do subscript index é um bug pré-existente que afetava qualquer
  programa que usasse variável como índice em assignment de array
  (ex.: `a[i] = v` onde `i` é variável local). Todos os exemplos
  existentes usavam literais ou `for` variables (que tinham disp
  correto por outro caminho), por isso o bug nunca tinha sido detectado.

## Testes

- `make -C swl-compiler test` → **73 passed, 0 failed**
- 38 exemplos + 35 rejeições negativas
- pipe_demo: saída "pipe ok\nchild\npipe demo ok\n" verificada
- Todos os testes existentes continuam passando (regressão zero)

## Integração

Qualquer IA que usar `a[i] = v` com `i` sendo variável local agora
funciona corretamente. Antes, o codegen gerava `[ebp+0]` em vez de
`[ebp+disp]` — se alguém encontrou esse comportamento, o fix já está
aplicado.

Builtins de IPC disponíveis: `swl_pipe`, `swl_dup`, `swl_dup2`.
Use `swl_pipe(fds)` passando um `var fds: [i32, 2]` — arrays
decayam para `*i32` automaticamente.
