MEMORY
{
   RAM (rwx) : ORIGIN = 0x00000, LENGTH = 65536
}

SECTIONS
{
   . = ORIGIN(RAM);
   .text : { *(.text) }
   .data : { *(.data) }
   .bss  : { *(.bss)  }

   PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM));
}

