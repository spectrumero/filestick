#ifndef SYSDEFS_H
#define SYSDEFS_H

#define DISABLE_INTERRUPTS \
   asm(".option arch, +zicsr\n\t" \
       "csrwi mstatus, 0");

#define ENABLE_INTERRUPTS \
   asm(".option arch, +zicsr\n\t" \
       "csrwi mstatus, 8");
#endif

