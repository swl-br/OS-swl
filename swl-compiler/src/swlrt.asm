; swlrt.asm -- SWL MVP runtime (x86 32-bit, Linux syscalls).
;
; This is the Assembly side of the "Assembly + C" compiler story:
;   - _start is the ELF entry point; it calls the compiled swl_main and
;     uses its return value as the process exit code;
;   - swl_print_i32 / swl_putchar / swl_exit are callable from SWL programs
;     (same calling convention the compiler emits: arguments on the stack,
;     callee keeps its own frame, no registers need saving beyond
;     eax/ecx/edx which our generated code treats as scratch).
;
; Build: nasm -f elf32 swlrt.asm -o swlrt.o
; Link:  ld -m elf_i386 swlrt.o program.o -o program

[bits 32]
section .text
global _start
extern swl_main

_start:
    call swl_main
    mov ebx, eax        ; exit status = main() result
    mov eax, 1          ; sys_exit
    int 0x80
.halt:
    jmp .halt           ; sys_exit never returns; belt and suspenders

; ----------------------------------------------------------------
; swl_exit(i32 code) -- never returns
; ----------------------------------------------------------------
global swl_exit
swl_exit:
    mov eax, [esp+4]
    mov ebx, eax
    mov eax, 1          ; sys_exit
    int 0x80
.halt:
    jmp .halt

; ----------------------------------------------------------------
; swl_putchar(i32 ch) -- writes one byte to stdout
; ----------------------------------------------------------------
global swl_putchar
swl_putchar:
    push ebp
    mov ebp, esp
    sub esp, 8
    mov eax, [ebp+8]
    mov byte [ebp-4], al
    mov edx, 1          ; length
    lea ecx, [ebp-4]    ; buffer
    mov ebx, 1          ; fd = stdout
    mov eax, 4          ; sys_write
    int 0x80
    add esp, 8
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_print_str(*u8 s) -- prints a NUL-terminated string
; ----------------------------------------------------------------
global swl_print_str
swl_print_str:
    push ebp
    mov ebp, esp
    sub esp, 16
    mov esi, [ebp+8]    ; pointer to string
    xor edx, edx        ; length
.len:
    cmp byte [esi+edx], 0
    je .emit
    inc edx
    jmp .len
.emit:
    mov ecx, esi        ; buffer
    mov ebx, 1          ; fd = stdout
    mov eax, 4          ; sys_write
    int 0x80
    add esp, 16
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_strlen(*u8 s) -> i32
; ----------------------------------------------------------------
global swl_strlen
swl_strlen:
    push ebp
    mov ebp, esp
    mov esi, [ebp+8]    ; pointer to string
    xor eax, eax        ; length
.len:
    cmp byte [esi+eax], 0
    je .done
    inc eax
    jmp .len
.done:
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_strcmp(*u8 a, *u8 b) -> i32  (<0 / 0 / >0 like libc strcmp)
; ----------------------------------------------------------------
global swl_strcmp
swl_strcmp:
    push ebp
    mov ebp, esp
    mov edi, [ebp+8]    ; a
    mov esi, [ebp+12]   ; b
    xor eax, eax
.loop:
    movzx ecx, byte [edi]
    movzx edx, byte [esi]
    cmp ecx, edx
    jne .diff
    test cl, cl
    jz .done
    inc edi
    inc esi
    jmp .loop
.diff:
    ; return (signed) difference of the first mismatching bytes
    mov eax, ecx
    sub eax, edx
    jmp .done
.done:
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_print_i32(i32 n) -- prints n as signed decimal + newline
; ----------------------------------------------------------------
global swl_print_i32
swl_print_i32:
    push ebp
    mov ebp, esp
    sub esp, 32

    mov eax, [ebp+8]    ; n
    test eax, eax
    jnz .nonzero

    ; n == 0
    mov edi, ebp
    sub edi, 1
    mov byte [edi], '0'
    dec edi             ; .emit does inc edi, so end one below the digit
    mov ecx, 1
    jmp .emit

.nonzero:
    xor ecx, ecx        ; digit count
    xor ebx, ebx        ; negative flag
    test eax, eax
    jns .digits
    neg eax
    mov ebx, 1

.digits:
    mov edi, ebp
    sub edi, 1          ; digits written downward from [ebp-1]
.loop:
    xor edx, edx
    mov esi, 10
    div esi             ; eax = eax/10, edx = remainder
    add dl, '0'
    mov byte [edi], dl
    dec edi
    inc ecx
    test eax, eax
    jnz .loop

    test ebx, ebx
    jz .emit
    mov byte [edi], '-'
    dec edi
    inc ecx

.emit:
    inc edi             ; edi -> first character
    mov edx, ecx        ; length
    mov ecx, edi        ; buffer
    mov ebx, 1          ; fd = stdout
    mov eax, 4          ; sys_write
    int 0x80

    mov byte [ebp-32], 10
    mov edx, 1
    lea ecx, [ebp-32]
    mov ebx, 1
    mov eax, 4
    int 0x80

    add esp, 32
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_print_u32(u32 n) -- prints n as unsigned decimal + newline
; (same digit loop as swl_print_i32 without the sign handling)
; ----------------------------------------------------------------
global swl_print_u32
swl_print_u32:
    push ebp
    mov ebp, esp
    sub esp, 32

    mov eax, [ebp+8]    ; n
    test eax, eax
    jnz .nonzero

    ; n == 0
    mov edi, ebp
    sub edi, 1
    mov byte [edi], '0'
    dec edi             ; .emit does inc edi, so end one below the digit
    mov ecx, 1
    jmp .emit

.nonzero:
    xor ecx, ecx        ; digit count
    mov edi, ebp
    sub edi, 1          ; digits written downward from [ebp-1]
.loop:
    xor edx, edx
    mov esi, 10
    div esi             ; unsigned divide: eax = eax/10, edx = remainder
    add dl, '0'
    mov byte [edi], dl
    dec edi
    inc ecx
    test eax, eax
    jnz .loop

.emit:
    inc edi             ; edi -> first character
    mov edx, ecx        ; length
    mov ecx, edi        ; buffer
    mov ebx, 1          ; fd = stdout
    mov eax, 4          ; sys_write
    int 0x80

    mov byte [ebp-32], 10
    mov edx, 1
    lea ecx, [ebp-32]
    mov ebx, 1
    mov eax, 4
    int 0x80

    add esp, 32
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_print_char(i32 ch) -- prints one raw byte, no newline
; (same behavior as swl_putchar; alias kept for naming symmetry)
; ----------------------------------------------------------------
global swl_print_char
swl_print_char:
    jmp swl_putchar

; ----------------------------------------------------------------
; swl_memset8(*u8 dst, i32 val, i32 n) -- fills n bytes of dst with
; (val & 0xff).  Returns nothing.
; ----------------------------------------------------------------
global swl_memset8
swl_memset8:
    push ebp
    mov ebp, esp
    push edi
    mov edi, [ebp+8]    ; dst
    mov eax, [ebp+12]   ; val
    mov ecx, [ebp+16]   ; n
    and eax, 0xFF       ; keep only the low byte
.fill:
    test ecx, ecx
    jz .done
    mov [edi], al
    inc edi
    dec ecx
    jmp .fill
.done:
    pop edi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_time_s() -> i32 -- seconds since the epoch (sys_time)
; ----------------------------------------------------------------
global swl_time_s
swl_time_s:
    xor ebx, ebx        ; tloc = NULL (we only want the return value)
    mov eax, 13         ; sys_time
    int 0x80
    ret

; ----------------------------------------------------------------
; swl_srand(i32 seed) / swl_rand() -> i32 -- xorshift32 PRNG.
; A zero seed is nudged to a nonzero constant; swl_rand seeds itself
; from the wall clock on the first call if swl_srand was never used.
; ----------------------------------------------------------------
global swl_srand
swl_srand:
    mov eax, [esp+4]
    test eax, eax
    jnz .store
    mov eax, 0x9E3779B9 ; golden-ratio nudge for a zero seed
.store:
    mov [swl_rand_seed], eax
    ret

global swl_rand
swl_rand:
    mov eax, [swl_rand_seed]
    test eax, eax
    jnz .have
    mov eax, 13         ; sys_time -> first-call self seed
    int 0x80
    or eax, 1           ; never leave the state at zero
    mov [swl_rand_seed], eax
.have:
    ; xorshift32: x ^= x<<13; x ^= x>>17; x ^= x<<5
    mov edx, eax
    shl edx, 13
    xor eax, edx
    mov edx, eax
    shr edx, 17
    xor eax, edx
    mov edx, eax
    shl edx, 5
    xor eax, edx
    mov [swl_rand_seed], eax
    ret

; ----------------------------------------------------------------
; swl_print_hex(i32 x) -- prints x as 0x prefix +8 hex digits + newline
; ----------------------------------------------------------------
global swl_print_hex
swl_print_hex:
    push ebp
    mov ebp, esp
    sub esp, 16

    ; print "0x" prefix
    mov word [ebp-16], 0x7830  ; '0','x' in little-endian
    mov edx, 2
    lea ecx, [ebp-16]
    mov ebx, 1
    mov eax, 4
    int 0x80

    ; convert 8 nibbles MSB -> LSB
    mov esi, [ebp+8]           ; value
    xor edi, edi               ; output index
.hexloop:
    mov ecx, 7
    sub ecx, edi
    shl ecx, 2                 ; ecx = (7 - i) * 4
    mov eax, esi
    shr eax, cl
    and eax, 0xF
    cmp al, 10
    jb .is_digit
    add al, 'a' - 10
    jmp .store
.is_digit:
    add al, '0'
.store:
    mov [ebp - 8 + edi], al
    inc edi
    cmp edi, 8
    jl .hexloop

    ; print 8 hex digits
    mov edx, 8
    lea ecx, [ebp-8]
    mov ebx, 1
    mov eax, 4
    int 0x80

    ; newline
    mov byte [ebp-16], 10
    mov edx, 1
    lea ecx, [ebp-16]
    mov ebx, 1
    mov eax, 4
    int 0x80

    add esp, 16
    pop ebp
    ret

section .bss
swl_rand_seed: resd 1
