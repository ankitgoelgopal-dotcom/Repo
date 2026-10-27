/* QAR-OS program entry: calls main(), then exit(main's return value). */
    .text
    .globl _start
_start:
    xor %ebp, %ebp
    call main
    mov %eax, %edi
    xor %eax, %eax          /* SYS_EXIT = 0 */
    int $0x80
1:  jmp 1b
    .section .note.GNU-stack,"",@progbits
