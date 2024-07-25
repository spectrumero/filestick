MEMORY
{
   RAM (rwx) : ORIGIN = 0x20000, LENGTH = 2048
}

SECTIONS
{
   . = ORIGIN(RAM);
   .text : { *(.text) }
   .data : { *(.data) }
   .bss  : { *(.bss)  }

   PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM));
}

