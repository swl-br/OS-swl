# Sessão: SWLC V2.6 — Builtin swl_strchr

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_strchr` ao runtime, completando o conjunto de busca em strings.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_strchr(s, ch) -> *u8`:
  - Signature: `(*u8, i32) -> i32` (retorna ponteiro como i32 na ABI)
  - Percorre s byte a byte comparando com `ch & 0xff`
  - Retorna ponteiro para o byte encontrado, ou NULL (0) se não encontrado
  - Também encontra NUL (ch=0) — retorna fim da string

### 2. Sema (sema.c)

- Adicionado `swl_strchr` ao array de builtins:
  - 2 argumentos: s (*u8), ch (i32)
  - Retorna T_I32 (ponteiro x86-32)

### 3. Exemplo (strchr_demo.swl)

- Cria s = "Hello"
- swl_strchr(s, 'l') → ponteiro para "llo", char='l', *p==108
- swl_strchr(s, 'H') → ponteiro para "Hello", char='H', *p==72
- swl_strchr(s, 'Z') → NULL
- swl_strchr(s, 0) → NUL (end-of-string)

## Resultado

```
summary: 64 passed, 0 failed
all tests passed
```

- 29 exemplos (incluindo strchr_demo)
- 35 rejeições

## Strings agora disponíveis

- `swl_strlen(s)` — tamanho
- `swl_strcmp(a, b)` — comparação
- `swl_strcpy(dst, src)` — cópia
- `swl_strcat(dst, src)` — concatenação
- `swl_strchr(s, ch)` — busca caractere
