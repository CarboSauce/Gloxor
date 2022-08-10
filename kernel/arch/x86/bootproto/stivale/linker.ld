OUTPUT_FORMAT("elf64-x86-64")
ENTRY(stivale2_main)
PHDRS
{
    null    PT_NULL    FLAGS(0) ;                   /* Null segment */
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
    rwx     PT_LOAD    FLAGS((1 << 0) | (1 << 1) | (1 << 2)) ;
                        /* Read Write Exec */
}
SECTIONS
{
    . = 0xffffffff80000000;
    kernelFileBegin = .;


    .text ALIGN(4K)  : {   
        *(.start*)  
        KEEP(*(.text*))
    } :text
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
        PROVIDE(_moduleTesting = .);
        KEEP(*(.module.test*))
        PROVIDE(_moduleTestingEnd = .);
    } :rodata

    .data ALIGN(4K) : {
        KEEP(*(.data*))

    } :data

    .bss ALIGN(4K) :{
        PROVIDE(bss_begin = .);
        KEEP(*(.bss*))
        *(COMMON)
        PROVIDE(bss_end = .);
    } :data

    .stivale2hdr ALIGN(4K) :
    {
        KEEP(*(.stivale2hdr))
    } :rodata
    kernelFileEnd = .;
/* 
    .interp : 
    {
        *(.interp*)
    } */
}
