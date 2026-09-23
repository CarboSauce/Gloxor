OUTPUT_FORMAT("elf64-x86-64")
ENTRY(limine_main)
PHDRS
{
    limine_requests    PT_LOAD    FLAGS((1 << 1) | (1 << 2)); /* Read + Write */
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)); /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2));            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)); /* Write + Read */
}
SECTIONS
{
    . = 0xffffffff80000000;
    kernelFileBegin = .;


    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text ALIGN(4K)  : {   
        *(.start*)  
        KEEP(*(.text*))
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata ALIGN(4K) : {
        *(.rodata*)
    } :rodata

    .init_array ALIGN(4K) : {
        PROVIDE( _ctorArrayStart = . );
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
        PROVIDE(_ctorArrayEnd = . );
    } :rodata

    .module ALIGN(4K): 
    {
        PROVIDE(_modulePreCpuBegin = .);
        KEEP(*(.module.precpu*))
        PROVIDE(_moduleDriverBegin = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.module.driver.*)))
        PROVIDE(_moduleDriverEnd = .);
        . = ALIGN(16); /* gcc wants allignment of 16 for whatever reason */
        PROVIDE(_moduleTesting = .);
        KEEP(*(.module.test*))
        PROVIDE(_moduleTestingEnd = .);
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data ALIGN(4K) : {
        KEEP(*(.data*))
    } :data

    .bss ALIGN(4K) :{
        PROVIDE(bss_begin = .);
        KEEP(*(.bss*))
        *(COMMON)
        PROVIDE(bss_end = .);
    } :data

    kernelFileEnd = .;

    /DISCARD/ : 
    {
        *(.interp*)
    }
}
