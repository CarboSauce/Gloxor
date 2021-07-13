OUTPUT_FORMAT("elf64-x86-64")
ENTRY(stivale2_main)
SECTIONS
{
    . = 0xffffffff80200000;
        
    .stivale2hdr ALIGN(4K) :
    {
        KEEP(*(.stivale2hdr))
    } 
    .text ALIGN(4K)  : {   
        *(.start*)  
        KEEP(*(.text*))
    }
    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .init_array ALIGN(4K) : {
        PROVIDE( _ctorArrayStart = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
        PROVIDE(_ctorArrayEnd = . );
    }

    .module ALIGN(4K): 
    {
        PROVIDE(_modulePreCpuBegin = .);
        KEEP(*(.module.precpu*))
        PROVIDE(_moduleDriverCentralBegin = .);
        KEEP(*(.module.central*))
        KEEP(*(.module.driver*))
        PROVIDE(_moduleDriverEnd = .);
    }

    .data ALIGN(4K) : {
        KEEP(*(.data*))
    }

    .bss ALIGN(4K) : {
        PROVIDE(bss_begin = .);
        KEEP(*(.bss*))
        *(COMMON)
        PROVIDE(bss_end = .);
    }
/* 
    .interp : 
    {
        *(.interp*)
    } */
}