OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start_core1)

/* Shared 16MiB a fine zona <0x4000_0000 */
PROVIDE(SHM_BASE = 0x3F000000);
PROVIDE(SHM_SIZE = 0x01000000);

/* Alias richiesti da HWLIB (alt_base.c) */
PROVIDE(_stack_top = __stack_top__);
PROVIDE(_vectors   = __vectors_start__);

MEMORY
{
  DDR_PRIV (rwx) : ORIGIN = 0x01000000, LENGTH = 0x3E000000   /* 0x0100_0000 .. 0x3EFF_FFFF */
  SHM      (rw ) : ORIGIN = 0x3F000000, LENGTH = 0x01000000   /* 16MiB condivisi */
}

SECTIONS
{
  . = ORIGIN(DDR_PRIV);

  /* Se metti i vettori in DDR: */
  .vectors :
  {
    . = ALIGN(0x100);
    __vectors_start__ = .;
    KEEP(*(.vectors))
    KEEP(*(.isr_vector))
    __vectors_end__ = .;
  } > DDR_PRIV

  .text :
  {
    . = ALIGN(4);
    __text_start__ = .;
    *(.text.startup*)
    *(.init)
    KEEP(*(.text.__core1_unwind_stub))
    *(.text*)
    *(.fini)
    __text_end__ = .;
  } > DDR_PRIV

 .ARM.extab :
  {
    . = ALIGN(4);
    __extab_start = .;
    KEEP(*(.ARM.extab*))
    __extab_end = .;
  } > DDR_PRIV
  
  .ARM.exidx :
  {
    . = ALIGN(4);
    __exidx_start = .;
     KEEP(*(.ARM.exidx*))
    __exidx_end = .;
  } > DDR_PRIV

  .rodata :
  {
    . = ALIGN(4);
    __rodata_start__ = .;
    *(.rodata*)
    __rodata_end__ = .;
  } > DDR_PRIV

  .data :
  {
    . = ALIGN(4);
    __data_start__ = .;
    *(.data*)
    __data_end__ = .;
  } > DDR_PRIV
  
   __data_load__ = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    __bss_end__ = .;
  } > DDR_PRIV

  /* Fine area “caricata” */
  . = ALIGN(16);
  __end__ = .;
  PROVIDE(end = __end__);

  /* Heap/stack markers */
  __heap_start__ = .;
  __heap_end__   = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV) - 0x00020000; /* ~128KiB per lo stack */

  __stack_top__   = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV);
  __stack_limit__ = __heap_end__;

  /* Sezione in SHM: allineamento dentro al blocco */
  .shm (NOLOAD) :
  {
    . = ALIGN(64);
    __shm_start__ = ORIGIN(SHM);
    *(.shm*)
    __shm_end__   = ORIGIN(SHM) + LENGTH(SHM);
  } > SHM

  /* Comodi */
  __ram_origin__ = ORIGIN(DDR_PRIV);
  __ram_end__    = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV);
}
