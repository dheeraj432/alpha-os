
BITS 32

global idt_load
global timer_irq_stub
global exception_stub_table

extern timer_handler
extern exception_handler

; Load the IDT register.
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; CPU exceptions WITHOUT a CPU-pushed error code.
%macro ISR_NOERR 1
global exception_stub_%1
exception_stub_%1:
    push dword 0
    push dword %1
    jmp exception_halt
%endmacro

; CPU exceptions WITH a CPU-pushed error code.
%macro ISR_ERR 1
global exception_stub_%1
exception_stub_%1:
    push dword %1
    jmp exception_halt
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; Report the exception vector and error code to C.
exception_halt:
    cli

    ; Stack initially contains:
    ; [esp]     = exception vector
    ; [esp + 4] = error code
    push dword [esp + 4]    ; error code
    push dword [esp + 4]    ; exception vector

    call exception_handler

.hang:
    hlt
    jmp .hang

; Table of addresses for exception vectors 0-31.
exception_stub_table:
%assign i 0
%rep 32
    dd exception_stub_%+i
%assign i i+1
%endrep

; Hardware timer IRQ0 handler.
timer_irq_stub:
    pushad
    cld

    call timer_handler

    ; Send EOI to the master PIC.
    mov al, 0x20
    out 0x20, al

    popad
    iretd

