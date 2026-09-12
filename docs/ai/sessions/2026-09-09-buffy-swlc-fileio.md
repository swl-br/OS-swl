# Sessão 2026-09-09 — Buffy / swlc V2.12 — file IO

**Escopo:** `swl_open` / `swl_read` / `swl_write` / `swl_close` / `swl_seek` / `swl_unlink` no runtime Assembly, registro no sema, correção do codegen (extern collector), exemplo `file_demo`, 70 testes.

## Runtime (`swl-compiler/src/swlrt.asm`)

Wrappers diretos para syscalls Linux i386:

| builtin | syscall | nº |
|---|---|---|
| `swl_open(path, flags, mode)` | `sys_open` | 5 |
| `swl_read(fd, buf, count)` | `sys_read` | 3 |
| `swl_write(fd, buf, count)` | `sys_write` | 4 |
| `swl_close(fd)` | `sys_close` | 6 |
| `swl_seek(fd, off, whence)` | `sys_lseek` | 19 |
| `swl_unlink(path)` | `sys_unlink` | 10 |

Cada wrapper é `push ebp / mov ebp,esp / mov ebx/ecx/edx / mov eax,n / int 0x80 / pop ebp / ret`. Retornos são `i32` (fd ou bytes; <0 em erro). `swl_write` aceita `buf` como `*u8` ou string literal (mesmo ponteiro).

## Sema (`swl-compiler/src/sema.c`)

```c
{ "swl_open",   3, T_I32, BP_PTR_U8, BP_I32, BP_I32 },
{ "swl_read",   3, T_I32, BP_I32, BP_PTR_U8, BP_I32 },
{ "swl_write",  3, T_I32, BP_I32, BP_PTR_U8, BP_I32 },
{ "swl_close",  1, T_I32, BP_I32, 0, 0 },
{ "swl_seek",   3, T_I32, BP_I32, BP_I32, BP_I32 },
{ "swl_unlink", 1, T_I32, BP_PTR_U8, 0, 0 },
```

## Codegen fix (`swl-compiler/src/codegen.c`)

O coletor de `extern` tinha `char ext[8][32]` com limite `*n < 8` e cobria apenas `S_VAR/S_RETURN/S_IF/S_WHILE/S_ASSIGN/S_EXPR`. Com 6 novos builtins + 5 antigos, `file_demo` usa 9 builtins distintos e o 9º (`swl_unlink`) caía fora da janela de 8 — erro `symbol swl_unlink not defined`.

Correções:
- `ext[8] -> ext[32]`, `*n < 8 -> *n < 32` em `collect_externs`.
- `collect_externs_stmt`: mesma troca `8->32`, adicionados `S_FOR`, `S_SWITCH`, `S_BREAK/CONTINUE/ELSEIF` (no-op).
- Sem isso, qualquer programa que usasse >8 builtins distintos falharia.

## Exemplo (`swl-compiler/examples/file_demo.swl`)

Constrói manualmente o path `/tmp/swl_file_demo.txt` byte a byte em `[u8,24]` (evita literais longas), depois:
- `open(W+CREAT+TRUNC=577, 420)` + `write("hello file\n",11)` + `close` → "write ok".
- `open(RDONLY=0)` + `read(buf,32)` → 11 bytes, NUL, `strcmp` → "read ok: hello file".
- `seek(6, SET=0)` + `read(buf2,4)` + `strcmp("file")` → "seek ok: file".
- `unlink` + "file ok".
Saída 4 linhas exatas, exit 0. Teste manual e `make test` ok.

## Validação

- `make -C swl-compiler clean && make -C swl-compiler && make -C swl-compiler rt` — ok.
- `make -C swl-compiler test` — **70 passed, 0 failed** (35 exemplos + 35 rejeições).
- Teste manual `swlc -> nasm -> ld -> ./file_demo_exe` — stdout exato, exit 0, arquivo removido.

## Docs

- `spec/mvp-subset.md` — seção V2.12 com flags documentadas.
- `README.md` — builtins + lista de exemplos atualizada.
- `PROJECT_STATE.md` — v2.12, 70 verificações, link da sessão.
