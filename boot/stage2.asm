[org 0x8000]
[bits 16]

SCRATCH_SEG   equ 0x1000
ZEROPAGE_SEG  equ 0x2000
ZEROPAGE_ADDR equ 0x00020000
KERNEL_DEST   equ 0x00100000
CMDLINE_SEG   equ 0x9000
CODE_SEL      equ 0x08
DATA_SEL      equ 0x10
HEADER_LEN    equ 0xA0

stage2_start:
    mov [boot_drive], dl

    mov si, msg_stage2
    call print_string

    mov word [dap.count], 1
    mov word [dap.offset], 0
    mov word [dap.segment], SCRATCH_SEG
    mov dword [dap.lba_low], 9
    mov dword [dap.lba_high], 0
    call read_lba

    mov ax, SCRATCH_SEG
    mov es, ax
    mov eax, [es:0]
    mov [kernel_lba], eax
    mov eax, [es:4]
    mov [kernel_sectors], eax
    mov eax, [es:8]
    mov [initrd_lba], eax
    mov eax, [es:12]
    mov [initrd_sectors], eax
    mov eax, [es:16]
    mov [initrd_bytes], eax

    ; -- le o setor de boot do kernel (so pra descobrir setup_sects) --
    mov word [dap.count], 1
    mov word [dap.offset], 0
    mov word [dap.segment], SCRATCH_SEG
    mov eax, [kernel_lba]
    mov [dap.lba_low], eax
    mov dword [dap.lba_high], 0
    call read_lba

    mov ax, SCRATCH_SEG
    mov es, ax
    movzx eax, byte [es:0x1F1]
    cmp eax, 0
    jne .have_setup_sects
    mov eax, 4
.have_setup_sects:
    inc eax
    mov [realmode_sectors], eax

    mov si, msg_chk1
    call print_string

    ; -- carrega o kernel protegido (32-bit) em memoria alta --
    ; (usa SCRATCH_SEG como rascunho - o setor de boot lido acima nao importa mais)
    mov eax, KERNEL_DEST
    mov [dest_addr], eax
    mov eax, [kernel_lba]
    add eax, [realmode_sectors]
    mov [cur_lba], eax
    mov eax, [kernel_sectors]
    sub eax, [realmode_sectors]
    mov [cur_sectors], eax
    call load_and_relocate

    mov si, msg_chk2
    call print_string

    ; -- carrega o initrd logo depois, alinhado --
    mov eax, [dest_addr]
    add eax, 0xFFF
    and eax, 0xFFFFF000
    mov [initrd_dest], eax
    mov [dest_addr], eax
    mov eax, [initrd_lba]
    mov [cur_lba], eax
    mov eax, [initrd_sectors]
    mov [cur_sectors], eax
    call load_and_relocate

    mov si, msg_ok
    call print_string

    ; -- AGORA le o setor de boot + codigo de setup de novo, por ultimo --
    ; (nada mais vai sobrescrever SCRATCH_SEG depois disso)
    mov word [dap.count], 1
    mov word [dap.offset], 0
    mov word [dap.segment], SCRATCH_SEG
    mov eax, [kernel_lba]
    mov [dap.lba_low], eax
    mov dword [dap.lba_high], 0
    call read_lba

    mov eax, [realmode_sectors]
    dec eax
    cmp eax, 0
    je .skip_setup_read
    mov word [dap.count], ax
    mov word [dap.offset], 0x200
    mov word [dap.segment], SCRATCH_SEG
    mov eax, [kernel_lba]
    inc eax
    mov [dap.lba_low], eax
    mov dword [dap.lba_high], 0
    call read_lba
.skip_setup_read:

    ; -- zera a area morta do setor de boot (antes do cabecalho), preserva o resto --
    mov ax, SCRATCH_SEG
    mov es, ax
    xor di, di
    xor ax, ax
    mov cx, 0xF8
    cld
    rep stosw

    mov si, msg_realmode_ok
    call print_string

    ; -- preenche os campos do cabecalho do kernel (patch nos campos do loader) --
    mov ax, SCRATCH_SEG
    mov es, ax

    mov byte [es:0x210], 0xFF

    mov eax, [initrd_dest]
    mov [es:0x218], eax
    mov eax, [initrd_bytes]
    mov [es:0x21C], eax

    mov ax, CMDLINE_SEG
    mov fs, ax
    mov si, cmdline_str
    xor di, di
.cmdline_copy:
    lodsb
    mov [fs:di], al
    inc di
    or al, al
    jnz .cmdline_copy

    mov dword [es:0x228], CMDLINE_SEG * 16

    mov si, msg_jump
    call print_string

    cli
    mov ax, SCRATCH_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0xE000

    jmp SCRATCH_SEG + 0x20:0x0000

[bits 16]
real_mode_hang:
    hlt
    jmp real_mode_hang

; ------------------------------------------------------------
load_and_relocate:
.loop:
    mov eax, [cur_sectors]
    cmp eax, 0
    je .done
    cmp eax, 64
    jbe .chunk_ok
    mov eax, 64
.chunk_ok:
    mov [chunk_sectors], eax

    mov ax, [chunk_sectors]
    mov word [dap.count], ax
    mov word [dap.offset], 0
    mov word [dap.segment], SCRATCH_SEG
    mov eax, [cur_lba]
    mov [dap.lba_low], eax
    mov dword [dap.lba_high], 0
    call read_lba

    mov eax, SCRATCH_SEG
    shl eax, 4
    call set_src_base
    mov eax, [dest_addr]
    call set_dst_base

    mov eax, [chunk_sectors]
    shl eax, 8
    mov cx, ax

    push es
    xor ax, ax
    mov es, ax
    mov si, gdt87
    mov ah, 0x87
    int 0x15
    pop es
    jc relocate_error

    mov eax, [chunk_sectors]
    add [cur_lba], eax
    sub [cur_sectors], eax
    mov eax, [chunk_sectors]
    shl eax, 9
    add [dest_addr], eax
    jmp .loop
.done:
    ret

read_lba:
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    ret

set_src_base:
    mov [src_base_low], ax
    shr eax, 16
    mov [src_base_mid], al
    mov [src_base_high], ah
    ret

set_dst_base:
    mov [dst_base_low], ax
    shr eax, 16
    mov [dst_base_mid], al
    mov [dst_base_high], ah
    ret

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

print_hex32:
    push eax
    push cx
    mov cx, 8
    mov ah, 0x0E
.digit:
    rol eax, 4
    push eax
    and eax, 0x0F
    cmp al, 10
    jl .num
    add al, 'A' - 10
    jmp .out
.num:
    add al, '0'
.out:
    int 0x10
    pop eax
    loop .digit
    pop cx
    pop eax
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp real_mode_hang

relocate_error:
    mov si, msg_relocate_error
    call print_string
    jmp real_mode_hang

boot_drive        db 0
kernel_lba        dd 0
kernel_sectors    dd 0
initrd_lba        dd 0
initrd_sectors    dd 0
initrd_bytes      dd 0
realmode_sectors  dd 0
cur_lba           dd 0
cur_sectors       dd 0
dest_addr         dd 0
initrd_dest       dd 0
chunk_sectors     dd 0

cmdline_str        db 'console=ttyS0', 0
newline            db 0x0D, 0x0A, 0
msg_pm32           db 'PM32-OK-JUMPING', 0

msg_stage2          db 'SWL OS - Estagio 2: lendo manifesto...', 0x0D, 0x0A, 0
msg_realmode_ok      db 'SWL OS - Setup 16-bit carregado.', 0x0D, 0x0A, 0
msg_chk1             db 'SWL OS - chk1: antes de carregar kernel/initrd.', 0x0D, 0x0A, 0
msg_chk2             db 'SWL OS - chk2: kernel em memoria alta.', 0x0D, 0x0A, 0
msg_ok               db 'SWL OS - Kernel e initrd em memoria alta.', 0x0D, 0x0A, 0
msg_jump             db 'SWL OS - Saltando para o kernel...', 0x0D, 0x0A, 0
msg_disk_error       db 'Erro ao ler disco.', 0x0D, 0x0A, 0
msg_relocate_error   db 'Erro ao mover memoria (INT 15h 87h).', 0x0D, 0x0A, 0

dap:
    .size     db 0x10
    .reserved db 0
    .count    dw 0
    .offset   dw 0
    .segment  dw 0
    .lba_low  dd 0
    .lba_high dd 0

align 8
flat_gdt_start:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
flat_gdt_end:

flat_gdt_descriptor:
    dw flat_gdt_end - flat_gdt_start - 1
    dd flat_gdt_start

align 8
gdt87:
    dd 0, 0
    dd 0, 0
src_limit     dw 0xFFFF
src_base_low  dw 0
src_base_mid  db 0
src_access    db 0x93
src_flags     db 0x00
src_base_high db 0
dst_limit     dw 0xFFFF
dst_base_low  dw 0
dst_base_mid  db 0
dst_access    db 0x93
dst_flags     db 0x00
dst_base_high db 0
    dd 0, 0
    dd 0, 0

times 4096-($-$$) db 0
