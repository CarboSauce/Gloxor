OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
SECTIONS
{
    . = 0xffffffff80200000;
        
    .stivale2hdr ALIGN(4K) :
    {
        *(.stivale2hdr)
    } 
    .text ALIGN(4K)  : {
        *(.start*)        
        *(.text*)
    }
    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .init_array ALIGN(4K) : {
        PROVIDE_HIDDEN( _ctor_array_start = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN(_ctor_array_end = . );
    }

    .data ALIGN(4K) : {
        *(.data*)
    }

    .bss ALIGN(4K) : {
        bss_begin = .;
        *(.bss*)
        *(COMMON)
        bss_end = .;
    }

/*     /DISCARD/ : {
        *(*)
    } */
}