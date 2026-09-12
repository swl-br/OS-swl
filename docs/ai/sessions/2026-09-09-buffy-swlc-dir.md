# Sessão 2026-09-09 — Buffy / swlc V2.13 — dir/process

**Escopo:** `swl_mkdir` / `swl_rmdir` / `swl_chdir` / `swl_getcwd` / `swl_getpid` no runtime Assembly, registro no sema, exemplo `dir_demo`, 71 testes.

## Runtime (`swl-compiler/src/swlrt.asm`)

Wrappers diretos para syscalls Linux i386:

| builtin | syscall | nº |
|---|---|---|
| `swl_mkdir(path, mode)` | `sys_mkdir` | 39 |
| `swl_rmdir(path)` | `sys_rmdir` | 40 |
| `swl_chdir(path)` | `sys_chdir` | 12 |
| `swl_getcwd(buf, size)` | `sys_getcwd` | 183 |
| `swl_getpid()` | `sys_getpid` | 20 |

Cada dir syscall é `push ebp / mov ebx/ecx / mov eax,n / int 0x80 / pop ebp / ret`. `getpid` é `mov eax,20 / int 0x80 / ret` sem frame. Retornos `i32` (0 ok, <0 erro; `getpid` >0; `getcwd` retorna `buf` ou 0).

## Sema (`swl-compiler/src/sema.c`)

```c
{ "swl_mkdir",  2, T_I32, BP_PTR_U8, BP_I32, 0 },
{ "swl_rmdir",  1, T_I32, BP_PTR_U8, 0, 0 },
{ "swl_chdir",  1, T_I32, BP_PTR_U8, 0, 0 },
{ "swl_getcwd", 2, T_I32, BP_PTR_U8, BP_I32, 0 },
{ "swl_getpid", 0, T_I32, 0, 0, 0 },
```

## Exemplo (`swl-compiler/examples/dir_demo.swl`)

Determinístico (não compara cwd/pid literais):

- `getpid()` >0 → "getpid ok".
- Constrói `/tmp/swl_dir_demo` byte a byte em `[u8,20]`, `mkdir(path, 493=0755)` → "mkdir ok".
- `getcwd(buf,64)` retorna `buf` !=0 e `strlen>0` → "getcwd ok".
- `chdir(path)` + `getcwd(buf2)` e `strcmp(buf,buf2)!=0` → "chdir ok".
- `chdir("/tmp")` + `rmdir(path)` → "dir ok".

Saída 5 linhas exatas, exit 0. Teste manual e `make test` ok.

## Validação

- `make -C swl-compiler clean && make -C swl-compiler && make -C swl-compiler rt` — ok.
- `make -C swl-compiler test` — **71 passed, 0 failed** (36 exemplos + 35 rejeições).

## Docs

- `spec/mvp-subset.md` — V2.12 expandida para "file IO + dir/process" com 5 novas linhas.
- `README.md` — builtins e lista de exemplos atualizada.
- `PROJECT_STATE.md` — v2.13, 71 verificações, link da sessão.
