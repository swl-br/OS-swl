[bits 32]

section .data
msg_start   db 'SWL OS - init', 0x0A
msg_start_len equ $ - msg_start

path_proc   db '/proc', 0
path_sys    db '/sys', 0
fs_proc     db 'proc', 0
fs_sys      db 'sysfs', 0

shell_path  db '/bin/sh', 0
shell_argv:
    dd shell_path
    dd 0
shell_envp:
    dd path_term
    dd 0
path_term   db 'TERM=linux', 0

section .bss
wait_status resd 1

section .text
global _start

_start:
    mov eax, 4
    mov ebx, 1
    mov ecx, msg_start
    mov edx, msg_start_len
    int 0x80

    mov eax, 39
    mov ebx, path_proc
    mov ecx, 0755o
    int 0x80

    mov eax, 39
    mov ebx, path_sys
    mov ecx, 0755o
    int 0x80

    mov eax, 21
    mov ebx, fs_proc
    mov ecx, path_proc
    mov edx, fs_proc
    mov esi, 0
    mov edi, 0
    int 0x80

    mov eax, 21
    mov ebx, fs_sys
    mov ecx, path_sys
    mov edx, fs_sys
    mov esi, 0
    mov edi, 0
    int 0x80

spawn_shell:
    mov eax, 2
    int 0x80
    cmp eax, 0
    je child_exec
    jl spawn_shell

parent_wait:
    mov eax, 114
    mov ebx, -1
    mov ecx, wait_status
    mov edx, 0
    mov esi, 0
    int 0x80
    jmp parent_wait

child_exec:
    mov eax, 11
    mov ebx, shell_path
    mov ecx, shell_argv
    mov edx, shell_envp
    int 0x80

    mov eax, 1
    mov ebx, 1
    int 0x80
