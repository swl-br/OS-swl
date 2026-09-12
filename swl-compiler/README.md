# swlc — compilador da linguagem SWL

Compilador da linguagem **SWL** para **x86 32-bit**, implementado em
**Assembly + C**. O frontend e o gerador de código são escritos em C e o
runtime/entrada do programa é Assembly puro (`src/swlrt.asm`). O backend
emite assembly compatível com o NASM usado no restante do SWL OS.

Estado atual:

- **MVP + M2 + M3 + M4 + M5 + M6 + M7 completos e verificados de ponta a
  ponta** (2026-09-09): a linguagem compila, monta (NASM), linka (ld) e
  roda um conjunto de 39 programas de exemplo e um conjunto de 35
  programas inválidos definidos no repositório (74 verificações no
  total).
- O `swlc` funciona como um compilador de um arquivo por execução:
  `swlc <arquivo>.swl` emite `<arquivo>.asm` na mesma pasta, e
  `swlc -o <saida>.asm <arquivo>.swl` controla o nome do arquivo de
  saída.
- O runtime é parte integrante do pipeline: todo programa compilado liga
  contra `src/swlrt.asm` e depende dos símbolos nele definidos
  (`swl_print_i32`, `swl_print_u32`, `swl_print_char`, `swl_putchar`,
  `swl_print_str`, `swl_strlen`, `swl_strcmp`, `swl_memset8`,
  `swl_time_s`, `swl_rand`, `swl_srand`, `swl_exit`).
- A linguagem é tipada, com verificação de tipos feita no frontend antes da
  geração de código, e os programas compilados têm a semântica de retorno
  pela saída definida em runtime (`swl_exit` e `exit`).
- O compartilhamento é feito executando `make test` dentro do diretório
  `swl-compiler/` antes de qualquer entrega; o runner atual testa o ciclo
  completo: compilação com `swlc`, montagem com NASM, linkagem com `ld -m
  elf_i386` e execução verificando saídas e códigos de retorno.

## Como usar

Do diretório raiz:

```bash
make -C swl-compiler       # compila o swlc e o runtime
make -C swl-compiler test  # executa o conjunto de testes
```

O `swlc` é gerado em `swl-compiler/build/swlc`.

Exemplo mínimo de compilação manual:

```bash
swl-compiler/build/swlc -o exemplo.asm exemplo.swl
nasm -f elf32 -o exemplo.o exemplo.asm
ld -m elf_i386 -o exemplo exemplo.o swl-compiler/build/swlrt.o
./exemplo
```

## O que a linguagem entrega agora

- Tipos inteiros com sinal e sem sinal (`i8`, `i16`, `i32`, `u8`,
  `u16`, `u32`, `isize`, `usize`).
- Tipos de ponteiro (`*i32`, `*u8`, `**i32`, ...) com deref de leitura
  e escrita (`*p`, `*p = v`, `**pp`).
- Structs nomeados com campos (incluindo structs aninhados), acesso
  `s.campo` e escrita `s.campo = v`. Atribuição por valor
  (`s1 = s2`) e inicialização (`var s: T = expr`), cópia via
  memcpy word-by-word. Structs por valor em parâmetros e retorno
  de funções (hidden pointer convention).
- Arrays locais com tamanho constante (`[T, N]`), leitura/escrita por
  índice (`a[i]`, `a[i] = v`, leitura encadeada `m[i][j]`) e
  inicialização de string para arrays de `u8`.
- Decay implícito de array local para ponteiro.
- Aritmética de ponteiro escalada (`p + i`, `i + p`, `p - i` com passo
  `sizeof(base)`), indexação de ponteiro na leitura (`p[i]`) e
  `ptr - int`.
- `sizeof(T)` para escalares, ponteiros, arrays e structs.
- Casts `as` entre inteiros (largura/sinal) e entre inteiro/ponteiro
  (reinterpretacão 32-bit), com `as` vinculando mais fraco que
  operadores unários (`*p as i32` é `(*p) as i32`).
- Operadores lógicos `and`/`or` (curto-circuito) e `not`, comparações
  completas, aritmética e divisão/mod com semântica signed/unsigned.
- Controle de fluxo: `if`/`else`, `while`, `for` (com e sem `var`),
  `switch`/`case`/`default`, `break`, `continue`, `return`;
  recursão suportada.
- Constantes `const` com valor inteiro.
- Variáveis globais `global name: T = init` no nível do módulo
  (inteiro ou string, zero-init se sem inicializador).
- Builtins do runtime: `swl_print_i32` (decimal + newline),
  `swl_print_u32` (decimal sem sinal + newline), `swl_print_char` e
  `swl_putchar` (um byte cru), `swl_print_str`, `swl_strlen`,
  `swl_strcmp`, `swl_strcpy` (copia string NUL-terminated, retorna dst),
  `swl_strcat` (concatena string NUL-terminated, retorna dst),
  `swl_strchr` (procura caractere, retorna ponteiro ou NULL),
  `swl_strncmp` (compara no máximo n bytes),
  `swl_itoa` (inteiro → string decimal, retorna buf),
  `swl_atoi` (string decimal → inteiro), `swl_malloc`/`swl_free`/
  `swl_realloc`/`swl_calloc` (heap via brk, ver spec),
  `swl_strdup` (duplica string),  `swl_open`/`swl_read`/`swl_write`/
  `swl_close`/`swl_seek`/`swl_unlink` (file IO),
  `swl_mkdir`/`swl_rmdir`/`swl_chdir`/`swl_getcwd`/`swl_getpid`/
  `swl_getppid`/`swl_fork`/`swl_waitpid`/`swl_exec`/`swl_sleep` (dir/process/fork),
  `swl_pipe`/`swl_dup`/`swl_dup2` (IPC: pipe, duplicação de fd),
  `swl_memset8` (preenche n bytes),
  `swl_memcpy` (copia n bytes entre ponteiros, retorna dst),
  `swl_memmove` (copia segura para sobreposição),  `swl_print_hex`
  (hexadecimal `0x` +8 dígitos), `swl_sprintf` (formatação em buffer:
  `%d %u %s %x %c %%`, até 3 args variádicos), `swl_time_s` (segundos desde a época),
  `swl_rand`/`swl_srand` (PRNG xorshift32 determinístico com semente) e
  `swl_exit`.

- **Bug fix V2.15**: corrigido sema — expressões de índice em
  subscript de assignment (`a[i] = v`) não eram resolvidas pelo sema,
  resultando em `disp=0` e codegen incorreto (`[ebp+0]` em vez de
  `[ebp+disp]`).

## Limitações

- Compilador de um único arquivo por execução; não há unidade de
  compilação múltipla.
- Heap é bump allocator via `brk` (free é no-op; não há coletor).
- O runtime é 32-bit e depende do ambiente de linkagem padrão do
  desenvolvimento.
- Não há tipos de 64 bits (`i64`/`u64`).
- Diferença entre ponteiros (`p - q`) não é suportada.

## Exemplos

Os exemplos estão em `swl-compiler/examples/`. Cada exemplo é um par
`<nome>.swl` e `<nome>.exit`; quando há saída de stdout esperada, existe
também `<nome>.out` e o runner testa a correspondência exata.

Exemplos disponíveis:

- `hello`, `fib`, `demo`, `bool`, `loops`, `ops`, `swap`, `structs`,
  `casts`, `unsigned`, `wide`, `layout`, `pointers`, `arrays`, `strcmp`,
  `putchar_demo`, `ptr_arith`, `u32_print`, `rand_demo`, `for_loop`,  `globals`, `hex_demo`, `for_reuse`, `struct_assign`, `struct_param`, `sprintf_demo`, `memcpy_demo`, `memmove_demo`, `str_copy_demo`, `strcat_demo`,
  `strchr_demo`, `strncmp_demo`, `itoa_demo`, `atoi_demo`, `heap_demo`,
  `file_demo`, `dir_demo`, `proc_demo`, `pipe_demo`.

## Testes negativos

Os testes negativos estão em `swl-compiler/tests/fail/`. Eles validam que
o compilador rejeita programas proibidos com mensagem no formato:

`arquivo:linha:col: error: ...`

## Documentação interna

- `swl-compiler/spec/mvp-subset.md` — subconjunto atual da linguagem.
- `swl-compiler/README.md` — visão geral do compilador.
- `docs/ai/PROJECT_STATE.md` — estado do projeto.
- `docs/ai/swlc-PLAN-001.md` — plano do compilador.
