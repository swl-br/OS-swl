# Sessão: SWLC V2.8 — Builtin swl_itoa

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_itoa` ao runtime, permitindo converter inteiros para strings decimais
sem printf.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_itoa(n, buf) -> buf`:
  - Signature: `(i32, *u8) -> *u8` (retorna buf)
  - buf precisa >=12 bytes (11 chars de "-2147483648" + NUL)
  - Trata n==0 como caso especial ("0")
  - Negativos: flag, neg (INT_MIN tratado via unsigned div)
  - Gera dígitos em temp na stack (ebp-1 downward), depois copia para buf
  - Assinatura no sema é `(i32, *u8)` — ptr é *u8, aceita decay de [u8,16]

### 2. Sema (sema.c)

- Adicionado `swl_itoa` ao array de builtins:
  - 2 argumentos: n (i32), buf (*u8)
  - Retorna T_I32

### 3. Exemplo (itoa_demo.swl)

- Testa 0, 42, -7, 123456, 2147483647
- Verifica cada resultado via swl_strcmp
- Printa "itoa <n>: <str>" para cada

## Resultado

```
summary: 66 passed, 0 failed
all tests passed
```

- 31 exemplos (incluindo itoa_demo)
- 35 rejeições

## Strings agora disponíveis

- `swl_strlen(s)` — tamanho
- `swl_strcmp(a, b)` — comparação
- `swl_strncmp(a, b, n)` — comparação limitada
- `swl_strcpy(dst, src)` — cópia
- `swl_strcat(dst, src)` — concatenação
- `swl_strchr(s, ch)` — busca caractere
- `swl_itoa(n, buf)` — inteiro → string decimal
