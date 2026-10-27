; Interrupt stubs, context switch and ring-3 entry for QAR-OS.
bits 64
extern isr_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 48
  %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 30
    ISR_ERR i
  %else
    ISR_NOERR i
  %endif
  %assign i i+1
%endrep
ISR_NOERR 128

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    cld
    call isr_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

section .rodata
global isr_table
isr_table:
%assign i 0
%rep 48
    dq isr %+ i
%assign i i+1
%endrep

section .text
; void switch_context(uint64_t *old_sp, uint64_t new_sp)
global switch_context
switch_context:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; void enter_user(uint64_t rip, uint64_t rsp)  -- drops to ring 3 via iretq
global enter_user
enter_user:
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    push qword 0x1B             ; ss  (user data, RPL3)
    push rsi                    ; rsp
    push qword 0x202            ; rflags: IF set
    push qword 0x23             ; cs  (user code, RPL3)
    push rdi                    ; rip
    iretq

global gdt_load
gdt_load:                       ; (ptr)
    lgdt [rdi]
    ret
global idt_load
idt_load:
    lidt [rdi]
    ret
global tss_load
tss_load:
    mov ax, 0x28
    ltr ax
    ret
section .note.GNU-stack noalloc noexec nowrite progbits
