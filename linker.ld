OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    _kernel_start = .;
    
    .text ALIGN(4096) : AT(ADDR(.text) - 0x100000)
    {
        _text_start = .;
        *(.text.entry)
        *(.text)
        *(.text.*)
        _text_end = .;
    }
    
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - 0x100000)
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }
    
    .data ALIGN(4096) : AT(ADDR(.data) - 0x100000)
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }
    
    .bss ALIGN(4096) : AT(ADDR(.bss) - 0x100000)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        . = ALIGN(8);
        _bss_end = .;
    }
    
    _kernel_end = .;
    
    /DISCARD/ :
    {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.debug*)
    }
}