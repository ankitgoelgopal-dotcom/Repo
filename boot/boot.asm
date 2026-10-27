; QAR-OS Phase 1 boot stub: Multiboot2 (GRUB) -> 64-bit long mode -> kmain()
; Identity-maps the first 1 GiB with 2 MiB pages. Phase 2 replaces this with a
; proper higher-half kernel mapping built by the memory manager.

MB2_MAGIC      equ 0xE85250D6
MB2_ARCH_I386  equ 0
MB2_BOOT_MAGIC equ 0x36D76289

section .multiboot
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd mb2_header_end - mb2_header
    dd -(MB2_MAGIC + MB2_ARCH_I386 + (mb2_header_end - mb2_header))
    dw 0                        ; end tag
    dw 0
    dd 8
mb2_header_end:

section .bss
align 4096
pml4:         resb 4096
pdpt:         resb 4096
pd:           resb 4096
align 16
stack_bottom: resb 32768
stack_top:

section .rodata
align 8
gdt64:
    dq 0                        ; null
    dq 0x00AF9A000000FFFF       ; 0x08 kernel code (64-bit, DPL0)
    dq 0x00CF92000000FFFF       ; 0x10 kernel data
gdt64_end:
gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .text
bits 32
global _start
extern kmain

_start:
    cli
    mov esp, stack_top
    mov edi, ebx                ; multiboot info pointer (-> rdi)
    mov esi, eax                ; bootloader magic      (-> rsi)
    cmp esi, MB2_BOOT_MAGIC
    jne .no_mb2

    mov eax, 0x80000000         ; long mode available?
    cpuid
    cmp eax, 0x80000001
    jb .no_long
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long

    mov eax, pdpt               ; PML4[0] -> PDPT
    or eax, 0b11
    mov [pml4], eax
    mov eax, pd                 ; PDPT[0] -> PD
    or eax, 0b11
    mov [pdpt], eax
    xor ecx, ecx
.map_pd:                        ; PD[i] = i * 2 MiB, huge page
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [pd + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd

    mov eax, pml4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5              ; PAE
    mov cr4, eax
    mov ecx, 0xC0000080         ; EFER
    rdmsr
    or eax, 1 << 8              ; LME
    wrmsr
    mov eax, cr0
    or eax, 1 << 31             ; PG
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_entry

.no_mb2:
    mov al, '1'
    jmp .fail
.no_long:
    mov al, '2'
.fail:
    mov dword [0xB8000], 0x4F524F45   ; "ER"
    mov dword [0xB8004], 0x4F3A4F52   ; "R:"
    mov byte  [0xB8008], al
    mov byte  [0xB8009], 0x4F
.halt32:
    hlt
    jmp .halt32

bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    xor rbp, rbp
    call kmain
.halt64:
    cli
    hlt
    jmp .halt64
