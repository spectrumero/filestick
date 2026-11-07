MEMORY
{
   RAM (rwx) : ORIGIN = 0x00000, LENGTH = 65536
}

SECTIONS
{
   . = ORIGIN(RAM);
   .text : { *(.text)
      __start = .;
   }
   .data : { *(.data) }
   .bss  : { *(.bss)  }

/* Only require __cons_buff when the software fifo is in use

   PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM) - 256);
   PROVIDE(__cons_buff = __stack_top); */

   PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM));

   __bss_start = ADDR(.bss);
   __bss_length = SIZEOF(.bss);

   /* somewhat of a hack, this is only used at the very start
      of init to clear down all uninitialised memory, before
      the stack is used */
   __free_length = 0x10000 - ADDR(.bss); 
}

