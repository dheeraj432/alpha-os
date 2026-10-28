bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, loading_message
    call print_string

    ; Read sector 2 into physical address 0x8000
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x8000
    int 0x13
    jc disk_error

    jmp 0x0000:0x8000

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

disk_error:
    mov si, error_message
    call print_string
.hang:
    cli
    hlt
    jmp .hang

boot_drive db 0
loading_message db "Loading Stage 2...", 0
error_message db "Disk read failed!", 0

times 510-($-$$) db 0
dw 0xAA55
