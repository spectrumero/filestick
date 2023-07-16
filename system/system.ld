MEMORY
{
   RAM (rwx) : ORIGIN = 0x10000, LENGTH = 67584
}

SECTIONS
{
   . = ORIGIN(RAM);
   .text : { *(.text)
      __start = .;
   }
   .data : { *(.data) }
   .bss  : { *(.bss)  }

   PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM));
}

