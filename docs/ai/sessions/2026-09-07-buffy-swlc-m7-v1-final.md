# Sessão 2026-09-07 (Buffy) — M7, fechamento de v1 e correções de coerência

## O que foi feito

Esta sessão fechou a v1 do compilador SWL. Partiu de um estado em que o
M6 estava recém-integrado e os docs prometiam símbolos de runtime que
não existiam no `swlrt.asm` (exatamente o "código fantasma" que os
critérios de v1 em `SWLC-008-criteria.md` e `NOTES-V1-SCOPE.md`
proíbem).

### M7 — builtins extras do runtime (implementados de verdade)

Antes: `README.md` listava `swl_print_u32`, `swl_print_char`,
`swl_memset8`, `swl_time_s`, `swl_rand` como símbolos do runtime, mas o
`swlrt.asm` só definia 6 funções. Em vez de apagar os docs, implementei
os builtins:

- `swl_print_u32(x: u32)` — decimal sem sinal + newline; verificado no
  limite de 32 bits (`-1 as u32` → `4294967295`; `65535*65535` →
  `4294836225`).
- `swl_print_char(ch: i32)` — byte cru sem newline (alias de
  `swl_putchar`, mesmo comportamento).
- `swl_memset8(dst: *u8, val: i32, n: i32)` — preenche n bytes com
  `val & 0xff`.
- `swl_time_s() -> i32` — segundos desde a época. **Bug real encontrado
  durante o teste**: `sys_time` precisa de `ebx` (tloc) = NULL; sem
  isso, `int 0x80` retornava `-EFAULT` e `t > 0` avaliava falso.
- `swl_srand(seed: i32)` / `swl_rand() -> i32` — xorshift32 com estado
  em `.bss`; seed zero vira `0x9E3779B9`; auto-seed pelo relógio na
  primeira chamada sem `swl_srand`. Sequência verificada contra uma
  implementação de referência (única diferença: `%` com sinal segue
  semântica C, resultado truncado pode ser negativo — correto).

Todos registrados na tabela de builtins do `sema.c` (novo tipo de
parâmetro `BP_U32` e suporte a 3 argumentos / zero argumentos). O
codegen não precisou mudar (emite `call <nome>`).

### Testes e exemplos novos

- `examples/u32_print.swl` + `.exit` + `.out` — borda unsigned.
- `examples/rand_demo.swl` + `.exit` + `.out` — PRNG determinístico
  (semente 42 → `32 -48 59 -80 -40`), memset8, strlen, time_s.
- `examples/putchar_demo` ganhou `.exit`/`.out` (antes rodava sem
  verificação de saída).
- `tests/fail/bad_builtin_u32.swl` — `swl_print_u32` rejeita `i32`.
- `tests/fail/bad_builtin_arity.swl` — aridade errada rejeitada.

### Bugs de coerência corrigidos

1. **`make clean` apagava fixtures de teste**: o alvo removia
   `examples/*.out`, que são arquivos versionados de saída esperada
   (não artefatos de build). Corrigido: `clean` agora só remove
   `examples/*.asm` e `examples/*.o`.
2. **Runner usava `diff -w`** (ignora espaços) enquanto os docs
   prometiam comparação byte a byte. Corrigido para `diff -q`.
3. **Binário fantasma** `bin/swlc` (35 KB, gitignored, de 05/09)
   removido junto com o diretório.
4. `docs/ai/PROJECT_STATE.md` tinha texto corrompido ("nível金融") e
   claim incorreto de M4 ("structs por valor"). Reescrito e alinhado.

## Estado final verificado

    make -C swl-compiler clean && make -C swl-compiler test
    → summary: 46 passed, 0 failed / all tests passed

19 exemplos + 27 rejeições. Build limpo do zero confirmado.

## Critério de v1 — avaliação

- compilador roda ✓; exemplos compilam/montam/linkam/rodam ✓;
- rejeição de inválidos observável ✓; sem código fantasma ✓;
- arquivos-chave coerentes entre si ✓; docs atualizados ✓.

**v1 está completa.**

## Próximos passos naturais (pós-v1)

1. Programas "de sistema" reais em SWL (tabela ASCII, utilitários).
2. Integração dos binários SWL com o userspace do SWL OS.
3. Pós-v1: strings em heap, múltiplas unidades, `for`/`switch`/globais.
