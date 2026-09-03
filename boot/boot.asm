[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_boot
    call print_string

    mov word [dap.count], 8
    mov word [dap.offset], 0x8000
    mov word [dap.segment], 0x0000
    mov dword [dap.lba_low], 1
    mov dword [dap.lba_high], 0
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x0000:0x8000

disk_error:
    mov si, msg_disk_error
    call print_string
.hang:
    hlt
    jmp .hang

print_string:
    mov ah, 0x0E
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

boot_drive     db 0
msg_boot       db 'SWL OS - Estagio 1 (LBA). Carregando Estagio 2...', 0x0D, 0x0A, 0
msg_disk_error db 'Erro ao ler disco (LBA).', 0x0D, 0x0A, 0

dap:
    .size     db 0x10
    .reserved db 0
    .count    dw 0
    .offset   dw 0
    .segment  dw 0
    .lba_low  dd 0
    .lba_high dd 0

times 510-($-$$) db 0
dw 0xAA55
