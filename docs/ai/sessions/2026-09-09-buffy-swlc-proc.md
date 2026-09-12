# Sessão 2026-09-09 — Buffy / swlc V2.14 — fork/exec/process

**Escopo:** `swl_getppid` / `swl_fork` / `swl_waitpid` / `swl_exec` / `swl_sleep` no runtime Assembly, registro no sema, exemplo `proc_demo`, 72 testes.

## Runtime (`swl-compiler/src/swlrt.asm`)

Wrappers diretos para syscalls Linux i386:

| builtin | syscall | nº | notas |
|---|---|---|---|
| `swl_getppid()` | `sys_getppid` | 64 | sem frame, só `mov eax,64 / int 0x80 / ret` |
| `swl_fork()` | `sys_fork` | 2 | idem |
| `swl_waitpid(pid, status_ptr, options)` | `sys_waitpid` | 7 | `status_ptr` é `*u8` (passa `p as *u8` onde `p: *i32 = &status`); 0 permitido |
| `swl_exec(path)` | `sys_execve` | 11 | constrói `argv=[path,NULL]` na stack, `env=NULL`; só retorna em erro |
| `swl_sleep(sec)` | `sys_nanosleep` | 162 | `timespec{tv_sec=sec, tv_nsec=0}`, `rem=NULL` |

`swl_exec` anota `argv[0]=path` em `[ebp-8]`, `argv[1]=NULL` em `[ebp-4]`, `lea ecx,[ebp-8]` como argv, `ebx=path`, `edx=0`.

## Sema (`swl-compiler/src/sema.c`)

```c
{ "swl_getppid", 0, T_I32, 0, 0, 0 },
{ "swl_fork",    0, T_I32, 0, 0, 0 },
{ "swl_waitpid", 3, T_I32, BP_I32, BP_PTR_U8, BP_I32 },
{ "swl_exec",    1, T_I32, BP_PTR_U8, 0, 0 },
{ "swl_sleep",   1, T_I32, BP_I32, 0, 0 },
```

`waitpid` status precisa `status as *u8` onde `status: i32` na stack e `p: *i32 = &status` → `p as *u8`.

## Exemplo (`swl-compiler/examples/proc_demo.swl`)

Determinístico, sem output variável:

- `getpid()>0` e `getppid()>0` → "pid ok".
- `fork()`: filho imprime "child" e `return 42`; pai `waitpid(pid, &status as *u8, 0)` e `status==10752` (42<<8) → "fork ok".
- Segundo `fork()+exec("/bin/true")` + `waitpid` e `status2==0` → "exec ok".
- `sleep(0)` → "sleep ok" + "proc ok".

Saída 6 linhas exatas, exit 0. `timeout 5` no runner evita hang.

## Validação

- `make -C swl-compiler clean && make -C swl-compiler && make -C swl-compiler rt` — ok.
- `make -C swl-compiler test` — **72 passed, 0 failed** (37 exemplos + 35 rejeições).
- Testes manuais `swlc->nasm->ld->./proc_demo_exe` — stdout exato, exit 0.

## Docs

- `spec/mvp-subset.md` — V2.12 expandida com 5 novas linhas (getppid/fork/waitpid/exec/sleep).
- `README.md` — builtins e lista de exemplos atualizada.
- `PROJECT_STATE.md` — v2.14, 72 verificações, link da sessão.
