# Sessão: SWLC V2.4 — Builtin swl_strcpy

Data: 2026-09-09
Responsável: Buffy

## Objetivo

Adicionar `swl_strcpy` ao runtime, complementando `swl_strcmp` e `swl_strlen`.

## O que foi feito

### 1. Runtime Assembly (swlrt.asm)

- Implementado `swl_strcpy(dst, src) -> dst`:
  - Signature: `(*u8, *u8) -> *u8`
  - Implementação: cópia byte a byte com verificação de NUL
  - Retorna dst para conveniência (como C strcpy)
  - Posicionado logo após `swl_strcmp`

### 2. Sema (sema.c)

- Adicionado `swl_strcpy` ao array de builtins:
  - 2 argumentos: dst (*u8), src (*u8)
  - Retorna T_I32 (ponteiro como i32 em x86-32)

### 3. Exemplo (str_copy_demo.swl)

- Demonstra cópia de string via swl_strcpy
- Verifica strlen do destino
- Verifica strcmp com original (0 = idênticas)
- Testa que swl_strcpy retorna ponteiro não-nulo

## Resultado

```
summary: 62 passed, 0 failed
all tests passed
```

- 27 exemplos (incluindo str_copy_demo)
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
swl_strcpy(dst, src)  ; copia "Hello\0"
```
