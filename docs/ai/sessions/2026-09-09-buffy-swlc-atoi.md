# Sessão: SWLC V2.9 — Builtin swl_atoi + fix string vazia

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_atoi` ao runtime (inverso de `swl_itoa`) e corrigir bug
pré-existente na emissão de strings vazias no codegen.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_atoi(s) -> i32`:
  - Signature: `(*u8) -> i32`
  - Pula whitespace (espaço, tab)
  - Aceita +/- opcional
  - Converte dígitos até primeiro não-dígito ou NUL
  - Overflow: detecta antes do multiply (compara com 214748364 = INT_MAX/10)
    ou via carry/overflow após add; cai pra INT_MAX ou INT_MIN
  - Retorna 0 se nenhum dígito encontrado

### 2. Sema (sema.c)

- Adicionado `swl_atoi` ao array de builtins:
  - 1 argumento: s (*u8)
  - Retorna T_I32

### 3. Fix codegen: string vazia

- Bug: string literal `""` gerava `db ,0` no NASM (sintaxe inválida)
- Causa: loop de bytes não executava quando len=0, resultando em `db ,0`
- Fix: se len==0, emite `db 0` diretamente

### 4. Exemplo (atoi_demo.swl)

- Testa: "42", "-7", "0", "", "  123", "+5", "99abc"
- Roundtrip via swl_itoa → swl_atoi com -12345

## Resultado

```
summary: 67 passed, 0 failed
all tests passed
```

- 32 exemplos (incluindo atoi_demo)
- 35 rejeições

## Conversão bidirecional completa

- `swl_itoa(n, buf)` — inteiro → string decimal
- `swl_atoi(s)` — string decimal → inteiro
