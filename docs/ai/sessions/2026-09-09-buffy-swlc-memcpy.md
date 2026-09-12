# Sessão: SWLC V2.3 — Builtin swl_memcpy

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_memcpy` ao runtime, complementando o `swl_memset8` já existente.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_memcpy(dst, src, n) -> dst`:
  - Signature: `(*u8, *u8, i32) -> *u8`
  - Implementação: `rep movsb` (cópia forward, eficiente)
  - Retorna dst para conveniência (como C memcpy)
  - Posicionado logo após `swl_memset8`

### 2. Sema (sema.c)

- Adicionado `swl_memcpy` ao array de builtins:
  - 3 argumentos: dst (*u8), src (*u8), n (i32)
  - Retorna T_I32 (ponteiro como i32 em x86-32)

### 3. Exemplo (memcpy_demo.swl)

- Demonstra cópia de array via swl_memcpy
- Verifica strlen do destino
- Testa que swl_memcpy retorna ponteiro não-nulo

## Resultado

```
summary: 61 passed, 0 failed
all tests passed
```

- 26 exemplos (incluindo memcpy_demo)
- 35 rejeições

## Uso

```swl
var src: [u8, 6]
src[0] = 72  ; H
src[1] = 101 ; e
src[2] = 108 ; l
src[3] = 108 ; l
src[4] = 111 ; o
src[5] = 0   ; NUL

var dst: [u8, 6]
swl_memcpy(dst, src, 6)  ; copia "Hello\0"
```
