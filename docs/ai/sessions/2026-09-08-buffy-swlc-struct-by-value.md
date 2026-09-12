# Sessão 2026-09-08 (Buffy) — Structs por valor completo

## O que foi feito

### Struct assignment e initialization

Implementação de structs por valor: cópia/atribuição de structs inteiros,
inicialização de variáveis struct com expressões do mesmo tipo, e acesso
a campos aninhados em structs.

#### Mudanças no sema (`sema.c`)

1. **`resolve_var_chain`**: Removida restrição "cannot use a whole struct
   as a value". Structs agora são valores válidos em expressões — o
   codegen carrega o endereço (como arrays).

2. **`S_ASSIGN`**: Permitida atribuição de struct inteiro (`s1 = s2`).
   A expressão do lado direito deve ter o mesmo tipo struct.

3. **`S_VAR` com init**: Permitida inicialização de variável struct
   com expressão do mesmo tipo (`var s: T = expr`).

4. **`E_ADDR`**: Removida restrição "cannot take address of struct".
   `&s` agora funciona para qualquer variável, incluindo structs.

5. **`E_UN` (dereference)**: Removida restrição "cannot dereference
   pointer to struct". `*p` funciona para ponteiros para structs.

6. **`resolve_deref_lval`**: Permitido armazenamento através de
   ponteiro para struct.

7. **`E_SUBSCRIPT`**: Permitido indexamento de arrays de structs.

8. **`require_value`**: Structs agora são aceitas como valores
   (não apenas inteiros/ponteiros).

#### Mudanças no codegen (`codegen.c`)

1. **`gen_struct_memcpy`**: Nova função que copia N bytes de [ecx]
   para [eax] word-by-word (4 bytes por iteração) + bytes restantes.
   Usada para atribuição e inicialização de structs.

2. **`E_LVAL`**: Quando o tipo é struct, carrega o endereço (como
   `E_ADDR`) em vez de tentar carregar o valor inteiro.

3. **`S_ASSIGN`**: Para structs, avalia a expressão (carrega endereço
   da fonte), calcula endereço do destino, e chama `gen_struct_memcpy`.

4. **`S_VAR` com init**: Para structs, mesma lógica que S_ASSIGN —
   avalia init, calcula endereço da variável, e faz memcpy.

### Limitações restantes

- Structs não são passados por valor como parâmetros de função
  (usar ponteiro: `fn f(p: *Point)`).
- Structs não são retornados por valor de funções.
- Arrays de structs: indexação funciona, mas atribuição de array
  inteiro continua restrita.

## Arquivos modificados

- `swl-compiler/src/sema.c` — 8 mudanças (ver acima).
- `swl-compiler/src/codegen.c` — `gen_struct_memcpy`, E_LVAL,
  S_ASSIGN, S_VAR.
- `swl-compiler/spec/mvp-subset.md` — structs por valor marcado.
- `swl-compiler/README.md` — features, limitações, exemplos.

## Arquivos novos

- `examples/struct_assign.swl` + `.exit` + `.out` — atribuição,
  inicialização, structs aninhados.
- `tests/fail/bad_struct_assign_type.swl` — type mismatch rejeitado.
- `tests/fail/bad_struct_init_type.swl` — init com tipo errado.

## Estado final

    make -C swl-compiler test
    → summary: 57 passed, 0 failed / all tests passed

24 exemplos, 33 rejeições. Build limpo confirmado.

## Decisões

- Struct memcpy word-by-word (4 bytes) em vez de `rep movsd` —
  mais simples, funciona para todos os tamanhos, e o overhead
  é insignificante para structs típicas (< 64 bytes).
- E_LVAL para structs carrega endereço (não valor) — consistente
  com decay de array para ponteiro em C.
- Struct params por valor ainda não implementados — manter
  simplicidade; structs são passados por ponteiro (convenção C
  para structs grandes).
