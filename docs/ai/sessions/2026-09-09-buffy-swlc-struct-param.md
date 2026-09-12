# Sessão: SWLC V2.2 — Structs por valor em parâmetros/retorno

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Implementar structs por valor em parâmetros e retorno de funções, completando a
feature de structs que havia ficado parcial (atribuição e inicialização já
funcionavam, mas structs não podiam ser passados ou retornados por valor).

## O que foi feito

### 1. Parser (parser.c)

- Removida a restrição `parse_type(p, 0)` em parâmetros de função
  → alterado para `parse_type(p, 1)` para aceitar structs sem ponteiro.
- Idem para tipo de retorno de função.

### 2. Sema (sema.c)

- Parâmetros struct agora são tratados como **hidden pointers**:
  - O slot na stack (`[ebp+8+4*pi]`) contém um ponteiro para o struct do caller.
  - O callee aloca uma cópia local (via `declare_local`).
  - Um `VarSym` auxiliar (`__ptr_<name>`) guarda o tipo ponteiro para o slot.
- Type checking funciona corretamente (rejeita int where Point expected).

### 3. Codegen (codegen.c)

- No prologue da função, após o zeroing do frame:
  - Para cada parâmetro struct, gera código que:
    1. Lê o ponteiro de `[ebp+8+4*pi]`
    2. Calcula o endereço da variável local
    3. Copia via `gen_struct_memcpy` (word-by-word)
- O caller já empurra o ponteiro corretamente (E_LVAL para struct carrega endereço).

### 4. Exemplos e testes

- **struct_param.swl**: exemplo completo com `make_point`, `add_points`,
  retorno de struct, múltiplos parâmetros.
- **bad_struct_param_type.swl**: rejeita `get_x(42)` onde `get_x` espera Point.
- **bad_struct_return_type.swl**: rejeita `p + 1` onde `p` é um Point.

## Resultado

```
summary: 60 passed, 0 failed
all tests passed
```

- 25 exemplos (incluindo struct_param)
- 35 rejeições (incluindo bad_struct_param_type, bad_struct_return_type)

## Limitações

- A abordagem hidden pointer copia o struct inteiro no prologue. Para structs
  grandes, isso pode ser ineficiente (mas é a abordagem padrão do C ABI em
  x86-32).
- Não há passagem de structs por referência explícita (use `*T` e `&s`).

## Próximos passos possíveis

1. Testes de regressão automatizados (comparador de saída).
2. Arrays de structs: atribuição de array inteiro.
3. Strings dinâmicas (malloc/free).
