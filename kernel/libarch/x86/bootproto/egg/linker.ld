OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
SECTIONS
{
    . = 0x200000;
        
    .start : {
        *(.start*)
        
    } 
    .text : {
        *(.text*)
    }

    .init_array : {
        PROVIDE_HIDDEN( _ctor_array_start = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN(_ctor_array_end = . );
    }

    .data : {
        *(.data*)
    }
    .rodata : {
        *(.rodata*)
    }
    .bss : {
        bss_begin = .;
        *(.bss*)
        *(COMMON)
        bss_end = .;
    }

    /DISCARD/ : {
        *(*)
    }
}