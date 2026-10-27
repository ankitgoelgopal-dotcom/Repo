/* Link script for QAR-OS user programs: load at 1 GiB, page-aligned segments. */
ENTRY(_start)
SECTIONS
{
    . = 0x40000000;
    .text   : ALIGN(4096) { *(.text .text.*) }
    .rodata : ALIGN(4096) { *(.rodata .rodata.*) }
    .data   : ALIGN(4096) { *(.data .data.*) }
    .bss    : ALIGN(4096) { *(COMMON) *(.bss .bss.*) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
