[bits 32]

section .data
msg_start   db 'SWL OS - init', 0x0A
msg_start_len equ $ - msg_start

path_proc   db '/proc', 0
path_sys    db '/sys', 0
path_dev    db '/dev', 0
fs_proc     db 'proc', 0
fs_sys      db 'sysfs', 0
fs_devtmpfs db 'devtmpfs', 0

; GUI-003: /run precisa existir (tmpfs, 0700) e XDG_RUNTIME_DIR precisa
; apontar pra ele ANTES de qualquer compositor/cliente Wayland rodar —
; sem isso o wl_display_create() do lado do swlwm (e de qualquer cliente)
; recusa iniciar.
path_run    db '/run', 0
fs_tmpfs    db 'tmpfs', 0

; GUI-003: tenta o swlwm primeiro. Se o execve falhar (binário ausente,
; lib faltando, etc. — cai direto pro código de erro do syscall, sem
; substituir o processo), o fluxo simplesmente continua pra baixo e
; tenta o /bin/sh em seguida. Isso garante que o boot nunca quebra por
; causa da GUI: na pior hipótese, cai no shell de sempre.
gui_path    db '/sbin/start-gui.sh', 0
gui_argv:
    dd gui_path
    dd 0

shell_path  db '/bin/sh', 0
shell_argv:
    dd shell_path
    dd 0
shell_envp:
    dd path_term
    dd path_xdgrt
    dd path_noinput
    dd 0
path_term   db 'TERM=linux', 0
path_xdgrt  db 'XDG_RUNTIME_DIR=/run', 0
; GUI-003: sem udev rodando de verdade, o libinput não consegue taguear
; os dispositivos (/dev/input/event*) como teclado/mouse (isso vem de
; regras de udev + hwdb, não só do device node existir via devtmpfs) —
; ele reporta 0 dispositivos e o wlroots recusa subir o backend por
; causa disso. Essa variável (sugerida pelo próprio log do wlroots)
; deixa o backend seguir mesmo sem input funcional ainda. Suporte a
; teclado/mouse de verdade é tarefa separada (precisa de um udev mínimo
; rodando, não é isto aqui).
path_noinput db 'WLR_LIBINPUT_NO_DEVICES=1', 0

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

    ; GUI-003-BUG: CONFIG_DEVTMPFS_MOUNT só auto-monta /dev quando o
    ; kernel troca pra um root real (disco) — num boot 100% initramfs
    ; (nosso caso) isso nunca acontece sozinho. Sem este mount, /dev
    ; só tem os 3 nós criados manualmente no build-rootfs.sh
    ; (console/null/tty) — nem /dev/tty0 nem /dev/dri/* existem, e o
    ; libseat não consegue abrir a VT pra ativar a sessão do swlwm.
    mov eax, 21
    mov ebx, fs_devtmpfs
    mov ecx, path_dev
    mov edx, fs_devtmpfs
    mov esi, 0
    mov edi, 0
    int 0x80

    ; /run — 0700, não 0755: é onde XDG_RUNTIME_DIR vai apontar, e o
    ; protocolo Wayland exige esse diretório restrito ao usuário dono
    ; (aqui, root — sistema ainda é single-user neste estágio).
    mov eax, 39
    mov ebx, path_run
    mov ecx, 0700o
    int 0x80

    mov eax, 21
    mov ebx, fs_tmpfs
    mov ecx, path_run
    mov edx, fs_tmpfs
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
    ; tenta a GUI primeiro
    mov eax, 11
    mov ebx, gui_path
    mov ecx, gui_argv
    mov edx, shell_envp
    int 0x80

    ; só chega aqui se o execve do swlwm falhou (registro em eax teria
    ; o -errno, mas não checamos o valor — qualquer falha cai pro shell)
    mov eax, 11
    mov ebx, shell_path
    mov ecx, shell_argv
    mov edx, shell_envp
    int 0x80

    ; os dois execve falharam — não há mais o que fazer
    mov eax, 1
    mov ebx, 1
    int 0x80
