# MVP e módulos implementados do SWL

Esta linguagem é projetada para ser compilada para x86-32 por um
frontend em C com runtime em Assembly.

## MVP

- módulos, fn, var, i32, u8, void, struct simples, literal de string,
  comentários, if, while, break, return.
- print_i32, print_str, putchar, exit como funções do runtime.

## M2 — ponteiros

- ponteiros puros para u8/i32.
- declaração com `&`, deref com `*`, endereço com `&`.
- sizeof, cast simples, uso em parâmetros.
- restrição razoável sobre ponteiros para struct e sobre uso de
  valor de ponteiro como expressão.

## M3 — arrays locais

- `var a: [u8, N]` com inicialização por literal de string.
- leitura `a[i]`, escrita `a[i] = v`, leitura encadeada `m[i][j]`.
- const e var local de array, índice inteiro.
- string longa demais é erro; atribuição inteira de array é rejeitada.
- array local decai para ponteiro quando passado onde se espera `*u8`.

## M4 — structs

- struct locais e em parâmetros com campo nomeado.
- `s.campo` e `s.campo = v`.
- struct por referência em parâmetro quando aplicável.
- rejeição razoável para uso como valor inteiro completo em contexto
  proibido.

## M5 — casts e tipos unsigned/wide

- casts explícitos entre tipos de base.
- valores unsigned e largos quando presentes.
- verificação de tipo coerente no uso.

## M6 — aritmética de ponteiro e sizeof

- `p + i`, `i + p` e `p - i` com passo escalado por `sizeof(base)`.
- indexação de ponteiro na leitura (`p[i]`); `ptr - int`.
- `sizeof(T)` para escalares, ponteiros, arrays e structs.
- casts `as` entre inteiros e entre inteiro/ponteiro (reinterpretacão
  32-bit); `as` vincula mais fraco que unários (`*p as i32` é
  `(*p) as i32`).
- proibido: `p * i`, `p / i`, `p % i`, `int - ptr`, `p - q` (diferença
  de ponteiros) e `p + q`.

## M7 — builtins extras do runtime

- `swl_print_u32(x: u32)` — decimal sem sinal + newline; aceita
  somente `u32` (use `x as u32` para reinterpretar).
- `swl_print_char(ch: i32)` — um byte cru, sem newline (mesmo
  comportamento de `swl_putchar`).
- `swl_memset8(dst: *u8, val: i32, n: i32)` — preenche n bytes com
  `val & 0xff`; arrays decaem para `*u8` no argumento.
- `swl_memcpy(dst: *u8, src: *u8, n: i32) -> i32` — copia n bytes de
  src para dst; retorna dst. Usa `rep movsb` (forward copy).
- `swl_memmove(dst: *u8, src: *u8, n: i32) -> i32` — copia n bytes de
  src para dst; retorna dst. Seguro para regiões sobrepostas (usa
  backward copy quando dst > src).
- `swl_strcpy(dst: *u8, src: *u8) -> *u8` — copia string NUL-terminated;
  retorna dst. Cópia byte a byte incluindo o NUL.
- `swl_strcat(dst: *u8, src: *u8) -> *u8` — concatena src ao fim de dst
  (NUL-terminated); retorna dst. Encontra o NUL de dst e copia src.
- `swl_strchr(s: *u8, ch: i32) -> i32` — procura `ch & 0xff` em `s`;
  retorna ponteiro para o byte encontrado ou `NULL` (0).
- `swl_strncmp(a: *u8, b: *u8, n: i32) -> i32` — compara no máximo n
  bytes; retorna <0, 0 ou >0 como `swl_strcmp`. Se `n == 0` sempre
  retorna 0.
- `swl_itoa(n: i32, buf: *u8) -> *u8` — converte inteiro com sinal
  para decimal NUL-terminated em `buf` (precisa ≥12 bytes); retorna
  `buf`. Trata `0`, negativos e `INT_MAX`.
- `swl_atoi(s: *u8) -> i32` — converte string decimal para i32;
  pula whitespace, aceita +/-, para no primeiro não-dígito;
  overflow cai pra INT_MAX/INT_MIN; retorna 0 se sem dígitos.
- `swl_malloc(n: i32) -> i32` / `swl_free(p: i32)` /
  `swl_realloc(p: i32, n: i32) -> i32` — heap dinâmico via `brk`
  (bump allocator; cada alocação `header + dados` arredondado a 4;
  `free` é no-op seguro; `realloc(NULL,n)=malloc(n)`, `realloc(p,0)=free`).
- `swl_calloc(nelem: i32, elsize: i32) -> i32` — `malloc(nelem*elsize)`
  zerado (usa `swl_memset8`).
- `swl_strdup(s: *u8) -> i32` — duplica string NUL-terminated (usa
  `swl_strlen + swl_malloc + swl_strcpy`).
- `swl_sprintf(buf: *u8, fmt: *u8, a1: i32, a2: i32, a3: i32) -> i32`
  — formatação em buffer; specifiers: `%d` (i32), `%u` (u32 como i32),
  `%s` (*u8), `%x` (hex), `%c` (byte), `%%` (literal). Sempre 5 args;
  args não consumidos são ignorados. Retorna bytes escritos (sem NUL).
- `swl_print_hex(x: i32)` — imprime como `0x` + 8 dígitos hex
  maiúsculos + newline (ex.: 255 → `0x000000ff`).
- `swl_time_s() -> i32` — segundos desde a época (sys_time).
- `swl_srand(seed: i32)` e `swl_rand() -> i32` — PRNG xorshift32;
  saída determinística após `swl_srand`, e auto-seed pelo relógio no
  primeiro `swl_rand()` sem semente. `%` em valores negativos segue
  semântica C (resultado pode ser negativo).

## V2.11 — heap dinâmico

- `swl_malloc`, `swl_calloc`, `swl_realloc`, `swl_free`, `swl_strdup`.

## V2.12 — file IO + dir/process + fork/exec (syscalls Linux i386)

- `swl_open(path: *u8, flags: i32, mode: i32) -> i32` — fd ou <0.
  Flags são as do Linux (ex.: 577 = `WRONLY|CREAT|TRUNC`).
- `swl_read(fd: i32, buf: *u8, count: i32) -> i32` — bytes lidos ou <0.
- `swl_write(fd: i32, buf: *u8, count: i32) -> i32` — bytes escritos.
- `swl_close(fd: i32) -> i32` — 0 ok, <0 erro.
- `swl_seek(fd: i32, offset: i32, whence: i32) -> i32` — novo offset.
- `swl_unlink(path: *u8) -> i32` — 0 ok, <0 erro.
- `swl_mkdir(path: *u8, mode: i32) -> i32` — 0 ok, <0 erro (mode 493=0755).
- `swl_rmdir(path: *u8) -> i32` — 0 ok, <0 erro.
- `swl_chdir(path: *u8) -> i32` — 0 ok, <0 erro.
- `swl_getcwd(buf: *u8, size: i32) -> i32` — retorna `buf` ou 0 em erro.
- `swl_getpid() -> i32` — pid do processo.
- `swl_getppid() -> i32` — pid do pai.
- `swl_fork() -> i32` — 0 no filho, pid do filho no pai, <0 erro.
- `swl_waitpid(pid: i32, status: *u8, options: i32) -> i32` — retorna
  pid do filho (status é `i32` bruto; use `status as *u8`; 42→10752=42<<8).
- `swl_exec(path: *u8) -> i32` — execve com `argv=[path,NULL], env=NULL`;
  só retorna em erro (<0), senão substitui o processo.
- `swl_sleep(sec: i32) -> i32` — nanosleep por sec segundos.

## V2.15 — IPC: pipe, dup, dup2 + fix subscript index

- `swl_pipe(fds: *i32) -> i32` — cria pipe anônimo; escreve dois fds
  em `fds` (fds[0]=read, fds[1]=write); retorna 0 ok, <0 erro.
  Syscall 42 (Linux i386).
- `swl_dup(oldfd: i32) -> i32` — duplica fd; retorna novo fd ou <0.
  Syscall 41.
- `swl_dup2(oldfd: i32, newfd: i32) -> i32` — duplica fd para número
  específico; retorna `newfd` ou <0. Syscall 63.
- **Fix**: expressões de índice em subscript de assignment (`a[i] = v`)
  agora são resolvidas pelo sema (antes `disp` ficava 0 por default,
  causando `[ebp+0]` incorreto no codegen).

## V2 — for, globais, hex e switch

- `for var i : T = start to limit [by step]` — loop com variável de
  loop declarada e tipada; `step` deve ser constante; suporta
  ascendente (step > 0, padrão) e descendente (step < 0); `break` e
  `continue` funcionam (`continue` vai direto pro step).
- `for i : T = start to limit [by step]` — reutiliza variável
  existente; a variável deve estar declarada antes do `for` e o tipo
  deve coincidir. Útil para iterar múltiplas vezes sem declarar nova
  variável a cada loop.
- `global name: T = init` — variável global no nível do módulo;
  inicialização por literal inteiro ou literal de string para arrays
  de `u8`; ausência de init → zero-init. Acessada por nome absoluto
  (`swl_g_<name>`) no assembly, sem frame de função.
- `switch expr case lit stmts ... [default stmts] end` — despacho
  por valor inteiro; case com literal inteiro ou char; default
  opcional; `break` sai do switch.
- `- [x]` structs por valor: atribuição (`s1 = s2`), inicialização
  (`var s: T = expr`), acesso a campos aninhados. Cópia via memcpy
  word-by-word no codegen. Structs por valor em parâmetros e retorno
  de funções (hidden pointer convention).

## Exemplo principal

`examples/arrays.swl`, `examples/strcmp.swl`,
`examples/ptr_arith.swl`, `examples/pointers.swl`,
`examples/structs.swl`, `examples/u32_print.swl`,
`examples/rand_demo.swl`, `examples/for_loop.swl`,
`examples/globals.swl`, `examples/hex_demo.swl` e outros no
 diretorio `examples/`.

## Status e dokumentação

- `README.md` no repositório do compilador.
- `docs/swlc/` com TOR, runbook, pipeline e status.
- `spec/mvp-subset.md` aqui.
