# Sessão: SWLC V2.7 — Builtin swl_strncmp

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_strncmp` ao runtime, estendendo as operações de comparação de strings.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_strncmp(a, b, n) -> i32`:
  - Signature: `(*u8, *u8, i32) -> i32` (<0 / 0 / >0 como strcmp)
  - Percorre a e b byte a byte, no máximo n bytes
  - Se n == 0 retorna 0 imediatamente
  - Se byte difere, retorna diferença signed
  - Se NUL encontrado antes de n, retorna diferença (ou 0 se ambos NUL)
  - Se n bytes sem diferença, retorna 0

### 2. Sema (sema.c)

- Adicionado `swl_strncmp` ao array de builtins:
  - 3 argumentos: a (*u8), b (*u8), n (i32)
  - Retorna T_I32
- Limpeza de warning: removidos `, 0` extras nas entradas de 0/1 arg

### 3. Exemplo (strncmp_demo.swl)

- a = "hello world", b = "hello there", c = "hello world"
- n=5: igual ("hello")
- n=6: igual ("hello ")
- n=7: difere (w vs t)
- n=0: sempre igual
- n=11: strings iguais completas

## Resultado

```
summary: 65 passed, 0 failed
all tests passed
```

- 30 exemplos (incluindo strncmp_demo)
- 35 rejeições

## Strings agora disponíveis

- `swl_strlen(s)` — tamanho
- `swl_strcmp(a, b)` — comparação
- `swl_strncmp(a, b, n)` — comparação limitada
- `swl_strcpy(dst, src)` — cópia
- `swl_strcat(dst, src)` — concatenação
- `swl_strchr(s, ch)` — busca caractere
