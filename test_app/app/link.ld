/*
 * linker script for GD32F30x with GNU ld
 * liYony 2022-09-23
 */

/* Program Entry, set to mark it as "used" and avoid gc */
/* Keep in sync with board.h */
APP_FLASH_BASE   = 0x08000000;
APP_FLASH_OFFSET = 0K;
APP_FLASH_ORIGIN = APP_FLASH_BASE + APP_FLASH_OFFSET;
APP_FLASH_LENGTH = 256K;
MEMORY
{
    CODE (rx) : ORIGIN = APP_FLASH_ORIGIN, LENGTH = APP_FLASH_LENGTH /* 256KB flash */
    RTT  (rw)  : ORIGIN = 0x20000000, LENGTH = 0x400  /* RTT buffer region */
    DATA  (rwx)  : ORIGIN = 0x20000400, LENGTH = 0xBA00 /* RAM region */
    APPVTOR (rw) : ORIGIN = 0x2000BE00, LENGTH = 0x200  /* Keep in sync with BOOT_APP_VTOR_RAM_* */
}
ENTRY(Reset_Handler)
_system_stack_size = 0x800;

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        __app_vector_start = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
        __app_vector_end = .;
    } > CODE = 0

    .app_reloc_info :
    {
        . = ALIGN(4);
        __app_reloc_info = .;
        LONG(0x524C4F43) /* "RLOC" */
        LONG(1)          /* version */
        LONG(16 * 4)     /* struct size */
        LONG(APP_FLASH_ORIGIN)
        LONG(__app_vector_start)
        LONG(__app_vector_end - __app_vector_start)
        LONG(ADDR(.rel.dyn))
        LONG(SIZEOF(.rel.dyn))
        LONG(8)
        LONG(_sidata)
        LONG(_sdata)
        LONG(_edata)
        LONG(__bss_start)
        LONG(__bss_end)
        /* Boot now always jumps into Reset_Handler. */
        LONG(Reset_Handler)
        /* Single-PIC-base (r9) is expected to use the data base, not GOT start. */
        LONG(_sdata)
        . = ALIGN(4);
    } > CODE = 0

    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text)                        /* remaining code */
        *(.text.*)                      /* remaining code */
        *(.rodata)                      /* read-only data (constants) */
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t*)

        . = ALIGN(4);
        _etext = .;
    } > CODE = 0

    /* .ARM.exidx is sorted, so has to go in its own output section.  */
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > CODE
    __exidx_end = .;

    .rel.dyn :
    {
        *(.rel.dyn)
        *(.rel.dyn.*)
    } > CODE

    .dynamic :
    {
        *(.dynamic)
    } > CODE

    /* This is used by the startup in order to initialize the .data secion */
    _sidata = .;

    /* .data section which is used for initialized data */

    .data : AT (_sidata)
    {
        . = ALIGN(4);
        /* This is used by the startup in order to initialize the .data secion */
        _sdata = . ;

        *(.data)
        *(.data.*)
        *(.data.rel.ro.local*)
        *(.data.rel.ro*)
        *(.gnu.linkonce.d*)

        /* finsh tables in RAM to avoid FLASH relocation targets */
        . = ALIGN(4);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        . = ALIGN(4);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;
        . = ALIGN(4);

        /* init tables in RAM to avoid FLASH relocation targets */
        . = ALIGN(4);
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;
        . = ALIGN(4);

        __app_got_start = .;
        *(.got)
        *(.got.*)
        *(.got.plt)
        __app_got_end = .;

        . = ALIGN(4);
        /* This is used by the startup in order to initialize the .data secion */
        _edata = . ;
    } >DATA

    /* Firmware size in flash */
    __firmware_start = APP_FLASH_ORIGIN;
    __firmware_end = _sidata + SIZEOF(.data);
    __firmware_size = __firmware_end - __firmware_start;

    __bss_start = .;

    .rtt (NOLOAD) : /* RTT buffer section */
    { /* Start .rtt */
        
        ; *(.jlink_rtt) /* RTT buffer data */
        *(.jlink_rtt_cb) /* RTT buffer data */
        . = ALIGN(4); /* Align end */
        *(.jlink_rtt_buf) /* RTT buffer data */
    } > RTT /* Place .rtt in RTT region */

    .app_vtor (NOLOAD) :
    {
        . = ALIGN(128);
        KEEP(*(.app_vtor))
        . = ALIGN(128);
    } > APPVTOR

    .bss :
    {
        . = ALIGN(4);
        /* This is used by the startup in order to initialize the .bss secion */
        _sbss = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        /* This is used by the startup in order to initialize the .bss secion */
        _ebss = . ;
        
        *(.bss.init)
    } > DATA
                                                                                                                                                                                                                                                                                                                                                                                                               
    .stack : 
    {
        . = . + _system_stack_size;
        . = ALIGN(4);
        _estack = .;
    } >DATA

    __bss_end = .;

    _end = .;

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /* DWARF debug sections.
     * Symbols in the DWARF debugging sections are relative to the beginning
     * of the section so we begin them at 0.  */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }

    ASSERT(SIZEOF(.app_vtor) <= 0x200, "APPVTOR region overflow")
}
