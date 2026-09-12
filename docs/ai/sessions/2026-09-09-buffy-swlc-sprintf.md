# Sessão — V2.16: builtin swl_sprintf + BP_ANY

**IA:** Buffy (Codebuff)
**Data:** 2026-09-09
**Responsável:** Implementação de swl_sprintf e documentação v2.16

## Objetivo

Adicionar `swl_sprintf` — formatação printf-like em buffer — e deixar
o sema capaz de aceitar tanto `i32` quanto ponteiros nos args variádicos.
Atualizar docs de v2.15 (73) para v2.16 (74).

## Alterações

### Arquivos modificados:
- `swl-compiler/src/swlrt.asm` — adicionada `swl_sprintf` (specifiers
  `%d %u %s %x %c %%`; helpers internos `int_to_str`, `uint_to_str`,
  `hex_to_str`; int_sign tratado; buffer NUL terminado; retorna bytes
  escritos sem o NUL)
- `swl-compiler/src/sema.c` — novo kind `BP_ANY = 4` para params que
  aceitam qualquer tipo sem checar; `swl_sprintf(buf, fmt, a1, a2, a3)`
  agora é `BP_PTR_U8, BP_PTR_U8, BP_ANY, BP_ANY, BP_ANY`
- `swl-compiler/src/codegen.c` — fix de coleta de `extern` (capacidade
  e cobertura) herdado da V2.12+ para novos builtins
- `swl-compiler/src/lexer.c`, `swl-compiler/src/parser.c` — nada de
  léxico; sprintf é nome de runtime reconhecido no sema
- `swl-compiler/src/swlc.h` — inclui BP_ANY no enum / tabela
- `swl-compiler/examples/sprintf_demo.swl` — novo exemplo cobrindo
  `%d`, `%s`, `%x`, `%u`, `%c` e `%%`
- `swl-compiler/examples/sprintf_demo.out` — fixture stdout
- `swl-compiler/examples/sprintf_demo.exit` — fixture 0
- `swl-compiler/README.md` — builtins, exemplos (39), contagem 74
- `swl-compiler/spec/mvp-subset.md` — seção V2.16
- `docs/ai/PROJECT_STATE.md` — v2.15→v2.16, 73→74 verificações

### Arquivos criados:
- `docs/ai/sessions/2026-09-09-buffy-swlc-sprintf.md` — esta sessão

## Implementação

### Assinatura

```c
swl_sprintf(buf: *u8, fmt: *u8, a1: i32/*ou *u8*/, a2: ..., a3: ...) -> i32
```

Sempre 5 args fixos no runtime. Args não consumidos pela string de
formato são ignorados. Retorna bytes escritos (sem contar o NUL final).

### Sema — BP_ANY

- Antes: `swl_sprintf` exigia `i32` nos três args variádicos; passar
  `*u8` (para `%s`) falhava no check `want != T_UNKNOWN && have != want`.
- Novo kind `BP_ANY` → `want = T_UNKNOWN` → skip de checagem; aceita
  `i32`, `*u8`, `*T`, `[]` decaído, literal. Não introduz `T_ANY` no
  sistema de tipos — só no dispatch de builtins.

### Runtime (swlrt.asm)

- Parse de `%` dígito a dígito; `%s` faz loop byte-a-byte até NUL;
  `%d`/`%u` via `div 10`; `%x` via `hex_to_str`; `%c` pega `al`;
  `%%` escreve `%`.
- Buffer bounds: garante NUL mesmo se a cópia for truncada (não usado
  em exemplo, mas presente).

### Exemplo sprintf_demo.swl

Formata strings, inteiros negativos, hex e char em buffer alocado via
`swl_malloc`; imprime com `swl_print_str`; saída verificada byte a byte.

## Decisões

- Manter os 5 args fixos simplifica o caller em SWL (sem variádico real)
  e mantém o runtime só com convenção x86 stdcall; futuras variantes
  poderiam ter overload com menos args mas não são necessárias para v1.
- `BP_ANY` é o menor delta que destrava `%s` sem relaxar todos os
  builtins — os outros builtins continuam com `BP_I32`/`BP_PTR_U8`/`BP_PTR_I32`.

## Testes

- `make -C swl-compiler test` → **74 passed, 0 failed**
- 39 exemplos + 35 rejeições negativas
- sprintf_demo.out verificado; regressão zero nos existentes
