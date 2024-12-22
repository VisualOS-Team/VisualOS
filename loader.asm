[BITS 16]
[ORG 0x9000]

start_loader:
    cli                      ; Disable interrupts
    xor ax, ax
    mov ds, ax               ; Initialize segments
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00           ; Initialize stack

    ; Debugging: Indicate transition to loader
    mov ah, 0x0E
    mov al, 'L'              ; Display 'L' for loader start
    int 0x10

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter protected mode
    mov eax, cr0
    or eax, 1                ; Enable PE in CR0
    mov cr0, eax

    ; Jump to protected mode
    jmp 0x08:protected_mode

[BITS 32]
protected_mode:
    ; Debugging: Indicate protected mode
    mov ah, 0x0E
    mov al, 'P'              ; Display 'P' for protected mode
    int 0x10

    ; Setup segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9C00

    ; Load kernel
    mov ax, 0x2000           ; Target segment for kernel (0x20000)
    mov es, ax
    xor bx, bx               ; Offset 0x0000
    mov dh, 0                ; Head
    mov ch, 0                ; Cylinder
    mov cl, 10               ; Sector start
    mov ah, 0x02             ; Read sectors
    mov al, 20               ; Approximate kernel size
    int 0x13
    jc error_kernel          ; Handle errors

    ; Debugging: Indicate kernel load success
    mov ah, 0x0E
    mov al, 'K'              ; Display 'K' for kernel
    int 0x10

    ; Jump to kernel
    jmp 0x2000:0x0000

error_kernel:
    ; Debugging: Indicate kernel error
    mov ah, 0x0E
    mov al, 'X'              ; Display 'X' for kernel error
    int 0x10

    ; Display error message
    mov si, kernel_error_msg
print_kernel_error:
    lodsb
    or al, al
    jz halt
    mov ah, 0x0E
    int 0x10
    jmp print_kernel_error

halt:
    cli
    hlt

; Error message
kernel_error_msg db "Kernel load error!", 0

; GDT
gdt:
    dq 0x0000000000000000      ; NULL descriptor
    dq 0x00CF9A000000FFFF      ; Code segment 32-bit
    dq 0x00CF92000000FFFF      ; Data segment

gdt_descriptor:
    dw gdt_end - gdt - 1
    dd gdt

gdt_end: