; kernel/src/kernel_entry.asm
BITS 64
GLOBAL _start
EXTERN kernel_main

SECTION .text
_start:
    ; RDI already contains our &kernel_params from the loader’s call
    CALL    kernel_main

.halt:
    CLI
    HLT
    JMP     .halt
