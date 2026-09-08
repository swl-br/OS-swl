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
- `swl_time_s() -> i32` — segundos desde a época (sys_time).
- `swl_srand(seed: i32)` e `swl_rand() -> i32` — PRNG xorshift32;
  saída determinística após `swl_srand`, e auto-seed pelo relógio no
  primeiro `swl_rand()` sem semente. `%` em valores negativos segue
  semântica C (resultado pode ser negativo).

## Exemplo principal

`examples/arrays.swl`, `examples/strcmp.swl`,
`examples/ptr_arith.swl`, `examples/pointers.swl`,
`examples/structs.swl`, `examples/u32_print.swl`,
`examples/rand_demo.swl` e outros no diretório `examples/`.

## Status e dokumentação

- `README.md` no repositório do compilador.
- `docs/swlc/` com TOR, runbook, pipeline e status.
- `spec/mvp-subset.md` aqui.
