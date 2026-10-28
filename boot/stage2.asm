bits 16
org 0x8000

CODE_SEG equ 0x08
DATA_SEG equ 0x10
MEMORY_MAP_ADDR equ 0x7000
MEMORY_MAP_MAX  equ 32

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save the BIOS boot drive
    mov [boot_drive], dl
    mov si, start_message
    call print_string
    ; Get the BIOS physical memory map using INT 15h E820.
    call get_memory_map

    ; Load the kernel from disk sector 3
    ; Destination physical address: 0x10000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 32
    mov ch, 0
    mov cl, 3
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    mov si, kernel_loaded_message
    call print_string

    ; Enter protected mode
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode

disk_error:
    mov si, error_message
    call print_string

halt:
    cli
    hlt
    jmp halt

print_string:
    lodsb
    test al, al
    jz .print_done
    mov ah, 0x0E
    int 0x10
    jmp print_string

.print_done:
    ret
; ------------------------------------------------------------
; Get BIOS E820 physical memory map
; Buffer at physical address 0x7000:
;   +0: entry count (16-bit)
;   +2: reserved
;   +4: entries, 24 bytes each
; ------------------------------------------------------------

get_memory_map:
    pushad
    push es
    push di
    push ds

    xor ax, ax
    mov es, ax
    mov ds, ax

    xor ebx, ebx
    mov di, MEMORY_MAP_ADDR + 4
    mov word [e820_count], 0

.e820_loop:
    cmp word [e820_count], MEMORY_MAP_MAX
    jae .done

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [es:di + 20], 1

    push ds
    push es
    push di
    int 0x15
    pop di
    pop es
    pop ds
    jc .done

    cmp eax, 0x534D4150
    jne .done

    cmp ecx, 20
    jb .done

    add di, 24
    inc word [e820_count]

    test ebx, ebx
    jnz .e820_loop

.done:
    mov ax, [e820_count]
    mov [MEMORY_MAP_ADDR], ax

    pop ds
    pop di
    pop es
    popad
    ret
bits 32

protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    ; Display a marker directly in VGA text memory.
    ; BIOS text interrupts are not used in protected mode.
    mov dword [0xB8000], 0x0F500F4D
    mov dword [0xB8004], 0x0F500F20

    ; Transfer execution to our C kernel
    mov eax, 0x10000
    jmp eax

bits 16

boot_drive db 0
start_message db "STAGE 2 STARTED", 13, 10, 0
kernel_loaded_message db "KERNEL LOADED", 13, 10, 0
error_message db "Kernel disk read failed!", 0
e820_count dw 0

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
