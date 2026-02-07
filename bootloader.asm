; Simple x86 bootloader that switches to 32-bit protected mode
; NASM syntax

[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    mov si, boot_msg
    call print_string

    ; Check for INT 13h extensions (LBA)
    mov bx, 0x55AA
    mov ah, 0x41
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error
    test cx, 0x0001
    jz disk_error

    ; Build DAP at 0x0600 and read KERNEL_SECTORS from LBA 1 into 0x1000
    mov byte [DAP_PTR], 0x10
    mov byte [DAP_PTR + 1], 0x00
    mov word [DAP_PTR + 2], KERNEL_SECTORS
    mov word [DAP_PTR + 4], 0x1000
    mov word [DAP_PTR + 6], 0x0000
    mov dword [DAP_PTR + 8], 1
    mov dword [DAP_PTR + 12], 0

    mov si, DAP_PTR
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

load_done:
    ; Enable A20 (fast A20 gate)
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; Set up GDT
    lgdt [gdt_descriptor]

    ; Enter protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:protected_mode

disk_error:
    mov si, disk_msg
    call print_string
    cli
    hlt
    jmp disk_error

[bits 32]
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Jump to kernel entry at 0x1000 (absolute far jump)
    jmp CODE_SEG:0x1000

; GDT
[bits 16]
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 35
%endif
DAP_PTR equ 0x0600
boot_drive: db 0

print_string:
    push ax
    push bx
    push si
.print_loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
    jmp .print_loop
.done:
    pop si
    pop bx
    pop ax
    ret

boot_msg: db 0x0D, 0x0A, "Booting OS...", 0x0D, 0x0A, 0
disk_msg: db 0x0D, 0x0A, "Disk read error.", 0x0D, 0x0A, 0

; Boot sector padding and signature
TIMES 510 - ($ - $$) db 0
DW 0xAA55

