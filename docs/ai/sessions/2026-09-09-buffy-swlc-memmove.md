# Sessão: SWLC V2.10 — Builtin swl_memmove

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_memmove` ao runtime, completando o conjunto de operações de memória
com suporte a regiões sobrepostas.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_memmove(dst, src, n) -> dst`:
  - Signature: `(*u8, *u8, i32) -> *u8` (retorna dst)
  - Se src == dst: retorna imediatamente
  - Se dst < src: forward copy (`cld` + `rep movsb`)
  - Se dst > src: backward copy (`std` + `rep movsb` do fim pro início)
  - Restaura direction flag (`cld`) após backward copy

### 2. Sema (sema.c)

- Adicionado `swl_memmove` ao array de builtins:
  - 3 argumentos: dst (*u8), src (*u8), n (i32)
  - Retorna T_I32 (ponteiro x86-32)

### 3. Exemplo (memmove_demo.swl)

- Forward overlap: "ABCDE" → copiar "ABC" 2 posições à direita → "ABABC"
- Backward overlap: copiar "ABC" de volta pro início → "ABCBC"
- Non-overlap: cópia pra buffer separado

## Resultado

```
summary: 68 passed, 0 failed
all tests passed
```

- 33 exemplos (incluindo memmove_demo)
- 35 rejeições

## Memória agora disponível

- `swl_memset8(dst, val, n)` — preenche bytes
- `swl_memcpy(dst, src, n)` — cópia forward (sem overlap)
- `swl_memmove(dst, src, n)` — cópia segura (com overlap)
