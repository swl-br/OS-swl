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
; swl_strcpy(dst, src) -> dst  -- copy NUL-terminated string
;   dst: *u8, src: *u8
;   copies including the NUL terminator; returns dst
; ----------------------------------------------------------------
global swl_strcpy
swl_strcpy:
    push ebp
    mov ebp, esp
    push edi
    push esi
    mov edi, [ebp+8]    ; dst
    mov esi, [ebp+12]   ; src
.loop:
    mov al, [esi]
    mov [edi], al
    test al, al
    jz .done
    inc edi
    inc esi
    jmp .loop
.done:
    mov eax, [ebp+8]    ; return dst
    pop esi
    pop edi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_strcat(dst, src) -> dst  -- append src to dst (NUL-terminated)
;   dst: *u8 (must have enough space), src: *u8
;   finds the NUL in dst, then copies src including its NUL; returns dst
; ----------------------------------------------------------------
global swl_strcat
swl_strcat:
    push ebp
    mov ebp, esp
    push edi
    push esi
    mov edi, [ebp+8]    ; dst
    mov esi, [ebp+12]   ; src
    ; find end of dst (NUL)
.find_end:
    mov al, [edi]
    test al, al
    jz .copy
    inc edi
    jmp .find_end
.copy:
    mov al, [esi]
    mov [edi], al
    test al, al
    jz .done
    inc edi
    inc esi
    jmp .copy
.done:
    mov eax, [ebp+8]    ; return dst
    pop esi
    pop edi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_strchr(s, ch) -> *u8  -- find first occurrence of ch in s
;   s: *u8, ch: i32 (only low byte used)
;   returns pointer to the byte, or NULL (0) if not found
; ----------------------------------------------------------------
global swl_strchr
swl_strchr:
    push ebp
    mov ebp, esp
    push esi
    mov esi, [ebp+8]    ; s
    mov dl, [ebp+12]    ; ch (low byte)
.loop:
    mov al, [esi]
    cmp al, dl
    je .found
    test al, al
    jz .notfound
    inc esi
    jmp .loop
.found:
    mov eax, esi
    jmp .done
.notfound:
    xor eax, eax        ; NULL
.done:
    pop esi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_strncmp(a, b, n) -> i32  (<0 / 0 / >0 like libc strncmp)
;   compares at most n bytes
; ----------------------------------------------------------------
global swl_strncmp
swl_strncmp:
    push ebp
    mov ebp, esp
    push esi
    push edi
    mov edi, [ebp+8]    ; a
    mov esi, [ebp+12]   ; b
    mov ecx, [ebp+16]   ; n
    test ecx, ecx
    jz .equal
.loop:
    movzx eax, byte [edi]
    movzx edx, byte [esi]
    cmp eax, edx
    jne .diff
    test al, al
    jz .equal
    inc edi
    inc esi
    dec ecx
    jnz .loop
.equal:
    xor eax, eax
    jmp .done
.diff:
    sub eax, edx
.done:
    pop edi
    pop esi
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
; swl_memcpy(dst, src, n) -> dst  -- copy n bytes from src to dst
;   dst: *u8, src: *u8, n: i32
;   returns dst for convenience (like C memcpy)
; ----------------------------------------------------------------
global swl_memcpy
swl_memcpy:
    push ebp
    mov ebp, esp
    push edi
    push esi
    mov edi, [ebp+8]    ; dst
    mov esi, [ebp+12]   ; src
    mov ecx, [ebp+16]   ; n
    ; direction flag is clear (forward copy)
    rep movsb           ; copy ecx bytes from [esi] to [edi]
    mov eax, [ebp+8]    ; return dst
    pop esi
    pop edi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_memmove(dst, src, n) -> dst  -- copy n bytes, safe for overlap
;   dst: *u8, src: *u8, n: i32
;   if dst < src: forward copy; if dst > src: backward copy
;   returns dst
; ----------------------------------------------------------------
global swl_memmove
swl_memmove:
    push ebp
    mov ebp, esp
    push edi
    push esi
    mov edi, [ebp+8]    ; dst
    mov esi, [ebp+12]   ; src
    mov ecx, [ebp+16]   ; n
    cmp edi, esi
    je .ret              ; src == dst: nothing to do
    ja .backward         ; dst > src: copy backwards
    ; forward copy (dst < src)
    cld
    rep movsb
    jmp .ret
.backward:
    ; backward copy: start from end
    std
    lea esi, [esi+ecx-1]   ; src end
    lea edi, [edi+ecx-1]   ; dst end
    rep movsb
    cld                     ; restore direction flag
.ret:
    mov eax, [ebp+8]    ; return dst
    pop esi
    pop edi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_atoi(s) -> i32  -- convert decimal string to signed i32
;   s: *u8, NUL-terminated decimal string
;   skips leading whitespace, handles +/-, stops at first non-digit
;   overflow clamps to INT_MIN/INT_MAX; returns 0 if no digits found
; ----------------------------------------------------------------
global swl_atoi
swl_atoi:
    push ebp
    mov ebp, esp
    push esi
    push ebx
    push edi
    mov esi, [ebp+8]            ; s
    xor edi, edi                ; result = 0
    xor ebx, ebx                ; sign flag: 0=positive, 1=negative
    ; skip whitespace (space, tab)
.skipws:
    movzx eax, byte [esi]
    cmp al, ' '
    je .wsok
    cmp al, 9                   ; tab
    je .wsok
    jmp .wsdone
.wsok:
    inc esi
    jmp .skipws
.wsdone:
    ; handle optional sign
    cmp byte [esi], '+'
    je .signok
    cmp byte [esi], '-'
    jne .digits
    mov ebx, 1                  ; negative
.signok:
    inc esi
    ; convert digits
.digits:
    xor ecx, ecx                ; found_digit flag
    ; overflow check constant: 214748364 (INT_MAX / 10)
.dloop:
    movzx eax, byte [esi]
    cmp al, '0'
    jb .done
    cmp al, '9'
    ja .done
    ; overflow check: if result > 214748364, will overflow
    cmp edi, 214748364
    ja .overflow
    ; result = result * 10
    mov eax, edi
    mov edx, 10
    mul edx                     ; edx:eax = result * 10
    jo .overflow
    mov edi, eax
    ; result += digit
    movzx eax, byte [esi]
    sub eax, '0'
    add edi, eax
    jc .overflow
    mov ecx, 1                  ; found at least one digit
    inc esi
    jmp .dloop
.overflow:
    ; clamp: if sign is negative, result = INT_MIN (-2147483648)
    ; else result = INT_MAX (2147483647)
    test ebx, ebx
    jnz .ovf_min
    mov edi, 2147483647
    jmp .done
.ovf_min:
    mov edi, 0x80000000         ; -2147483648 in two's complement
    jmp .done
.done:
    test ecx, ecx
    jnz .apply_sign
    ; no digits found: return 0
    xor edi, edi
    jmp .ret
.apply_sign:
    test ebx, ebx
    jz .ret
    neg edi
.ret:
    mov eax, edi
    pop edi
    pop ebx
    pop esi
    pop ebp
    ret

; ----------------------------------------------------------------
; swl_itoa(n, buf) -> buf  -- convert signed i32 to decimal string
;   n: i32, buf: *u8 (must have >=12 bytes)
;   writes NUL-terminated decimal without newline; returns buf
; ----------------------------------------------------------------
global swl_itoa
swl_itoa:
    push ebp
    mov ebp, esp
    sub esp, 32
    push edi
    push esi
    push ebx
    mov eax, [ebp+8]        ; n
    mov edx, [ebp+12]       ; buf
    mov [ebp-32], edx       ; save original buf
    test eax, eax
    jnz .nonzero
    ; n == 0
    mov edx, [ebp-32]
    mov byte [edx], '0'
    mov byte [edx+1], 0
    mov eax, edx
    jmp .done
.nonzero:
    xor ecx, ecx            ; digit count
    xor ebx, ebx            ; negative flag
    test eax, eax
    jns .digits
    mov ebx, 1
    neg eax                 ; 0x80000000 stays 0x80000000 -> 2147483648 unsigned
.digits:
    mov edi, ebp
    sub edi, 1              ; temp write pointer at ebp-1, grows downward
.loop:
    xor edx, edx
    mov esi, 10
    div esi                 ; eax/=10, edx=remainder
    add dl, '0'
    mov [edi], dl
    dec edi
    inc ecx
    test eax, eax
    jnz .loop
    test ebx, ebx
    jz .copy
    mov byte [edi], '-'
    dec edi
    inc ecx
.copy:
    inc edi                 ; edi -> first char in temp
    mov esi, edi            ; src
    mov edi, [ebp-32]       ; dst = buf
    mov edx, ecx            ; count
    test edx, edx
    jz .nul
.copylp:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    dec edx
    jnz .copylp
.nul:
    mov edi, [ebp-32]
    add edi, ecx
    mov byte [edi], 0
    mov eax, [ebp-32]       ; return buf
.done:
    pop ebx
    pop esi
    pop edi
    add esp, 32
    pop ebp
    ret

; ----------------------------------------------------------------
; Heap — bump allocator via brk (sys_brk = 45)
; ----------------------------------------------------------------
; Each allocation: [u32 size][data...] rounded to 4 bytes.
; Free is a no-op in this MVP (leaks but safe). realloc mallocs,
; copies min(old,new) and "frees" old.
; Pointers are raw addresses returned as i32.

global swl_malloc
swl_malloc:
    push ebp
    mov ebp, esp
    push ebx
    push edi
    push esi
    mov eax, [ebp+8]        ; n
    test eax, eax
    jle .malloc_null
    add eax, 4              ; header
    add eax, 3
    and eax, 0xFFFFFFFC      ; round to 4
    mov edi, eax            ; total
    mov eax, [swl_heap_ptr]
    test eax, eax
    jnz .malloc_have
    ; init: brk(0)
    xor ebx, ebx
    mov eax, 45
    int 0x80
    mov [swl_heap_start], eax
    mov [swl_heap_end], eax
    mov [swl_heap_ptr], eax
.malloc_have:
    mov eax, [swl_heap_ptr]
    lea ecx, [eax+edi]
    cmp ecx, [swl_heap_end]
    jbe .malloc_no_brk
    mov ebx, ecx
    mov eax, 45
    int 0x80
    cmp eax, ebx
    jb .malloc_null
    mov [swl_heap_end], eax
.malloc_no_brk:
    mov eax, [swl_heap_ptr]
    mov edx, edi
    sub edx, 4              ; usable size
    mov [eax], edx
    add eax, 4              ; user pointer
    add dword [swl_heap_ptr], edi
    jmp .malloc_done
.malloc_null:
    xor eax, eax
.malloc_done:
    pop esi
    pop edi
    pop ebx
    pop ebp
    ret

global swl_free
swl_free:
    push ebp
    mov ebp, esp
    ; no-op: we keep bump pointer monotonic.
    ; Could validate range but not needed for MVP.
    pop ebp
    ret

global swl_realloc
swl_realloc:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov esi, [ebp+8]        ; old ptr
    mov edi, [ebp+12]       ; new_n
    test esi, esi
    jz .realloc_malloc
    test edi, edi
    jz .realloc_free
    push edi
    call swl_malloc
    add esp, 4
    test eax, eax
    jz .realloc_done
    mov ebx, eax            ; new ptr
    mov ecx, [esi-4]        ; old usable size
    cmp ecx, edi
    jbe .realloc_copy
    mov ecx, edi
.realloc_copy:
    push ecx
    push esi
    push ebx
    call swl_memcpy
    add esp, 12
    push esi
    call swl_free
    add esp, 4
    mov eax, ebx
    jmp .realloc_done
.realloc_malloc:
    push edi
    call swl_malloc
    add esp, 4
    jmp .realloc_done
.realloc_free:
    push esi
    call swl_free
    add esp, 4
    xor eax, eax
.realloc_done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; ----------------------------------------------------------------
; File IO wrappers (Linux i386 syscalls)
; ----------------------------------------------------------------
global swl_open
swl_open:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; path *u8
    mov ecx, [ebp+12]   ; flags i32
    mov edx, [ebp+16]   ; mode i32
    mov eax, 5          ; sys_open
    int 0x80
    pop ebp
    ret

global swl_read
swl_read:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; fd
    mov ecx, [ebp+12]   ; buf *u8
    mov edx, [ebp+16]   ; count
    mov eax, 3          ; sys_read
    int 0x80
    pop ebp
    ret

global swl_write
swl_write:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; fd
    mov ecx, [ebp+12]   ; buf *u8
    mov edx, [ebp+16]   ; count
    mov eax, 4          ; sys_write
    int 0x80
    pop ebp
    ret

global swl_close
swl_close:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; fd
    mov eax, 6          ; sys_close
    int 0x80
    pop ebp
    ret

global swl_seek
swl_seek:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; fd
    mov ecx, [ebp+12]   ; offset
    mov edx, [ebp+16]   ; whence (0=SET,1=CUR,2=END)
    mov eax, 19         ; sys_lseek
    int 0x80
    pop ebp
    ret

global swl_unlink
swl_unlink:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; path *u8
    mov eax, 10         ; sys_unlink
    int 0x80
    pop ebp
    ret

; ----------------------------------------------------------------
; Dir/process wrappers (Linux i386 syscalls)
; ----------------------------------------------------------------
global swl_mkdir
swl_mkdir:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; path *u8
    mov ecx, [ebp+12]   ; mode i32
    mov eax, 39         ; sys_mkdir
    int 0x80
    pop ebp
    ret

global swl_rmdir
swl_rmdir:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; path *u8
    mov eax, 40         ; sys_rmdir
    int 0x80
    pop ebp
    ret

global swl_chdir
swl_chdir:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; path *u8
    mov eax, 12         ; sys_chdir
    int 0x80
    pop ebp
    ret

global swl_getcwd
swl_getcwd:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; buf *u8
    mov ecx, [ebp+12]   ; size i32
    mov eax, 183        ; sys_getcwd
    int 0x80
    pop ebp
    ret

global swl_getpid
swl_getpid:
    mov eax, 20         ; sys_getpid
    int 0x80
    ret

; ----------------------------------------------------------------
; Process control wrappers (Linux i386 syscalls)
; ----------------------------------------------------------------
global swl_getppid
swl_getppid:
    mov eax, 64         ; sys_getppid
    int 0x80
    ret

global swl_fork
swl_fork:
    mov eax, 2          ; sys_fork
    int 0x80
    ret

global swl_waitpid
swl_waitpid:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; pid
    mov ecx, [ebp+12]   ; status ptr (i32 raw, 0 allowed)
    mov edx, [ebp+16]   ; options
    mov eax, 7          ; sys_waitpid
    int 0x80
    pop ebp
    ret

global swl_exec
swl_exec:
    push ebp
    mov ebp, esp
    sub esp, 8
    mov eax, [ebp+8]    ; path *u8
    mov [ebp-8], eax    ; argv[0] = path
    mov dword [ebp-4], 0 ; argv[1] = NULL
    lea ecx, [ebp-8]    ; argv
    mov ebx, eax        ; path
    xor edx, edx        ; envp = NULL
    mov eax, 11         ; sys_execve
    int 0x80
    add esp, 8
    pop ebp
    ret

global swl_sleep
swl_sleep:
    push ebp
    mov ebp, esp
    sub esp, 8
    mov eax, [ebp+8]    ; sec
    mov [ebp-8], eax    ; tv_sec
    mov dword [ebp-4], 0 ; tv_nsec
    lea ebx, [ebp-8]    ; req
    xor ecx, ecx        ; rem = NULL
    mov eax, 162        ; sys_nanosleep
    int 0x80
    add esp, 8
    pop ebp
    ret

; ----------------------------------------------------------------
; Pipe wrappers (Linux i386 syscalls)
; ----------------------------------------------------------------
global swl_pipe
swl_pipe:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; fds *i32 (int[2])
    mov eax, 42         ; sys_pipe
    int 0x80
    pop ebp
    ret

global swl_dup
swl_dup:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; oldfd
    mov eax, 41         ; sys_dup
    int 0x80
    pop ebp
    ret

global swl_dup2
swl_dup2:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]    ; oldfd
    mov ecx, [ebp+12]   ; newfd
    mov eax, 63         ; sys_dup2
    int 0x80
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
; swl_calloc(nelem, elsize) -> i32  -- malloc + zero
; swl_strdup(s) -> i32             -- dup string
; ----------------------------------------------------------------
global swl_calloc
swl_calloc:
    push ebp
    mov ebp, esp
    push ebx
    mov eax, [ebp+8]        ; nelem
    mov ebx, [ebp+12]       ; elsize
    mul ebx                 ; edx:eax = total
    test edx, edx
    jnz .calloc_null
    push eax
    call swl_malloc
    add esp, 4
    test eax, eax
    jz .calloc_done
    mov ebx, eax
    mov eax, [ebp+8]
    mul dword [ebp+12]
    push eax
    push dword 0
    push ebx
    call swl_memset8
    add esp, 12
    mov eax, ebx
    jmp .calloc_done
.calloc_null:
    xor eax, eax
.calloc_done:
    pop ebx
    pop ebp
    ret

global swl_strdup
swl_strdup:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    mov esi, [ebp+8]        ; s
    push esi
    call swl_strlen
    add esp, 4
    inc eax                 ; +NUL
    push eax
    call swl_malloc
    add esp, 4
    test eax, eax
    jz .dup_done
    push esi
    push eax
    call swl_strcpy
    add esp, 8
.dup_done:
    pop esi
    pop ebx
    pop ebp
    ret

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

; ----------------------------------------------------------------
; swl_sprintf(buf, fmt, a1, a2, a3) -> i32  -- formatted write to buf
;   buf: *u8  -- destination buffer (must be large enough)
;   fmt: *u8  -- format string
;   a1, a2, a3: i32  -- arguments consumed left-to-right
;   Format specifiers:
;     %d  -- signed decimal (i32)
;     %u  -- unsigned decimal (u32)
;     %s  -- string (*u8, NUL-terminated)
;     %x  -- lowercase hex (u32)
;     %c  -- single character (low byte of i32)
;     %%  -- literal '%'
;   Returns number of bytes written (excluding NUL).
;   NUL is always appended.
; ----------------------------------------------------------------
global swl_sprintf
swl_sprintf:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    sub esp, 80

    mov eax, [ebp+8]
    mov [ebp-16], eax      ; buf_start
    mov eax, [ebp+16]
    mov [ebp-20], eax      ; a1
    mov eax, [ebp+20]
    mov [ebp-24], eax      ; a2
    mov eax, [ebp+24]
    mov [ebp-28], eax      ; a3
    mov dword [ebp-32], 0  ; arg_idx

    mov edi, [ebp-16]      ; destination
    mov esi, [ebp+12]      ; fmt

.L_fmt_loop:
    lodsb
    test al, al
    jz .L_done
    cmp al, '%'
    jne .L_emit
    lodsb
    test al, al
    jz .L_done
    cmp al, '%'
    je .L_pct
    cmp al, 'd'
    je .L_int
    cmp al, 'u'
    je .L_uint
    cmp al, 's'
    je .L_str
    cmp al, 'x'
    je .L_hex
    cmp al, 'c'
    je .L_char
    ; unknown specifier -> emit '%' and the char
    mov byte [edi], '%'
    inc edi
    mov [edi], al
    inc edi
    jmp .L_fmt_loop
.L_pct:
    mov byte [edi], '%'
    inc edi
    jmp .L_fmt_loop
.L_emit:
    mov [edi], al
    inc edi
    jmp .L_fmt_loop

; ---- %d signed ----
.L_int:
    mov edx, [ebp-32]
    cmp edx, 0
    je .L_int_a1
    cmp edx, 1
    je .L_int_a2
    cmp edx, 2
    je .L_int_a3
    xor eax, eax
    jmp .L_int_got
.L_int_a1: mov eax, [ebp-20]
    jmp .L_int_got
.L_int_a2: mov eax, [ebp-24]
    jmp .L_int_got
.L_int_a3: mov eax, [ebp-28]
.L_int_got:
    inc dword [ebp-32]
    push esi
    mov esi, eax
    lea ecx, [ebp-40]
    mov byte [ecx], 0
    test esi, esi
    jnz .L_int_nz
    dec ecx
    mov byte [ecx], '0'
    jmp .L_int_copy
.L_int_nz:
    xor ebx, ebx
    test esi, esi
    jns .L_int_pos
    mov ebx, 1
    neg esi
.L_int_pos:
    mov eax, esi
.L_int_div:
    xor edx, edx
    push ecx
    mov ecx, 10
    div ecx
    pop ecx
    add dl, '0'
    dec ecx
    mov [ecx], dl
    mov esi, eax
    test esi, esi
    jnz .L_int_div
    test ebx, ebx
    jz .L_int_copy
    dec ecx
    mov byte [ecx], '-'
.L_int_copy:
    mov al, [ecx]
    test al, al
    jz .L_int_done
    mov [edi], al
    inc edi
    inc ecx
    jmp .L_int_copy
.L_int_done:
    pop esi
    jmp .L_fmt_loop

; ---- %u unsigned ----
.L_uint:
    mov edx, [ebp-32]
    cmp edx, 0
    je .L_uint_a1
    cmp edx, 1
    je .L_uint_a2
    cmp edx, 2
    je .L_uint_a3
    xor eax, eax
    jmp .L_uint_got
.L_uint_a1: mov eax, [ebp-20]
    jmp .L_uint_got
.L_uint_a2: mov eax, [ebp-24]
    jmp .L_uint_got
.L_uint_a3: mov eax, [ebp-28]
.L_uint_got:
    inc dword [ebp-32]
    push esi
    mov esi, eax
    lea ecx, [ebp-40]
    mov byte [ecx], 0
    test esi, esi
    jnz .L_uint_nz
    dec ecx
    mov byte [ecx], '0'
    jmp .L_uint_copy
.L_uint_nz:
    mov eax, esi
.L_uint_div:
    xor edx, edx
    push ecx
    mov ecx, 10
    div ecx
    pop ecx
    add dl, '0'
    dec ecx
    mov [ecx], dl
    mov esi, eax
    test esi, esi
    jnz .L_uint_div
.L_uint_copy:
    mov al, [ecx]
    test al, al
    jz .L_uint_done
    mov [edi], al
    inc edi
    inc ecx
    jmp .L_uint_copy
.L_uint_done:
    pop esi
    jmp .L_fmt_loop

; ---- %s string ----
.L_str:
    mov edx, [ebp-32]
    cmp edx, 0
    je .L_str_a1
    cmp edx, 1
    je .L_str_a2
    cmp edx, 2
    je .L_str_a3
    xor eax, eax
    jmp .L_str_got
.L_str_a1: mov eax, [ebp-20]
    jmp .L_str_got
.L_str_a2: mov eax, [ebp-24]
    jmp .L_str_got
.L_str_a3: mov eax, [ebp-28]
.L_str_got:
    inc dword [ebp-32]
    test eax, eax
    jz .L_fmt_loop
    push esi
    mov esi, eax
.L_str_loop:
    lodsb
    test al, al
    jz .L_str_end
    mov [edi], al
    inc edi
    jmp .L_str_loop
.L_str_end:
    pop esi
    jmp .L_fmt_loop

; ---- %x hex ----
.L_hex:
    mov edx, [ebp-32]
    cmp edx, 0
    je .L_hex_a1
    cmp edx, 1
    je .L_hex_a2
    cmp edx, 2
    je .L_hex_a3
    xor eax, eax
    jmp .L_hex_got
.L_hex_a1: mov eax, [ebp-20]
    jmp .L_hex_got
.L_hex_a2: mov eax, [ebp-24]
    jmp .L_hex_got
.L_hex_a3: mov eax, [ebp-28]
.L_hex_got:
    inc dword [ebp-32]
    push esi
    mov esi, eax
    lea ecx, [ebp-40]
    mov byte [ecx], 0
    test esi, esi
    jnz .L_hex_nz
    dec ecx
    mov byte [ecx], '0'
    jmp .L_hex_copy
.L_hex_nz:
    mov eax, esi
    mov esi, eax
.L_hex_div:
    mov eax, esi
    xor edx, edx
    push ecx
    mov ecx, 16
    div ecx
    pop ecx
    cmp dl, 10
    jb .L_hex_digit
    add dl, 'a' - 10
    jmp .L_hex_store
.L_hex_digit:
    add dl, '0'
.L_hex_store:
    dec ecx
    mov [ecx], dl
    mov esi, eax
    test esi, esi
    jnz .L_hex_div
.L_hex_copy:
    mov al, [ecx]
    test al, al
    jz .L_hex_done
    mov [edi], al
    inc edi
    inc ecx
    jmp .L_hex_copy
.L_hex_done:
    pop esi
    jmp .L_fmt_loop

; ---- %c char ----
.L_char:
    mov edx, [ebp-32]
    cmp edx, 0
    je .L_char_a1
    cmp edx, 1
    je .L_char_a2
    cmp edx, 2
    je .L_char_a3
    xor eax, eax
    jmp .L_char_got
.L_char_a1: mov eax, [ebp-20]
    jmp .L_char_got
.L_char_a2: mov eax, [ebp-24]
    jmp .L_char_got
.L_char_a3: mov eax, [ebp-28]
.L_char_got:
    inc dword [ebp-32]
    mov [edi], al
    inc edi
    jmp .L_fmt_loop

.L_done:
    mov byte [edi], 0
    mov eax, edi
    sub eax, [ebp-16]
    add esp, 80
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret


section .bss

swl_rand_seed: resd 1
swl_heap_start: resd 1
swl_heap_end:   resd 1
swl_heap_ptr:   resd 1
