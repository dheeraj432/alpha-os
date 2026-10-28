BITS 32

global context_switch

; void context_switch(unsigned int *old_esp,
;                     unsigned int new_esp);
;
; Save the current context and restore another task's context.
; The new stack must contain:
;   edi, esi, ebx, ebp, return_address
; at its current stack pointer.

context_switch:
    push ebp
    push ebx
    push esi
    push edi

    ; First argument: address where the old ESP is stored.
    mov eax, [esp + 20]
    mov [eax], esp

    ; Second argument: new task's saved ESP.
    mov esp, [esp + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; Start a task after its initial context is restored.
global task_bootstrap
extern task_run_current
extern task_exit_current

task_bootstrap:
    call task_run_current
    call task_exit_current

.task_halt:
    cli
    hlt
    jmp .task_halt
