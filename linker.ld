ENTRY(_start)
SECTIONS {
    . = 1M;
    .text : { KEEP(*(.multiboot)) *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }

    /* The 'end' symbol marks the first byte of free RAM after the kernel */
    _end = .;
    end = .; 

    /DISCARD/ : { *(.note.gnu.build-id) *(.note.gnu.property) }
}
