# Sessão: SWLC V2.5 — Builtin swl_strcat

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_strcat` ao runtime, completando o trio básico de strings.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_strcat(dst, src) -> dst`:
  - Signature: `(*u8, *u8) -> *u8`
  - Encontra o NUL em dst, depois copia src incluindo seu NUL
  - Retorna dst para conveniência (como C strcat)

### 2. Sema (sema.c)

- Adicionado `swl_strcat` ao array de builtins:
  - 2 argumentos: dst (*u8), src (*u8)
  - Retorna T_I32 (ponteiro como i32 em x86-32)

### 3. Exemplo (strcat_demo.swl)

- Inicia dst com "Hello"
- Concatena " World" → "Hello World" (len=11)
- Verifica strcmp
- Concatena "!" → "Hello World!" (len=12)

## Resultado

```
summary: 63 passed, 0 failed
all tests passed
```

- 28 exemplos (incluindo strcat_demo)
- 35 rejeições

## Strings agora disponíveis

- `swl_strlen(s)` — tamanho
- `swl_strcmp(a, b)` — comparação
- `swl_strcpy(dst, src)` — cópia
- `swl_strcat(dst, src)` — concatenação
