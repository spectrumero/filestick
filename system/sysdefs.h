#ifndef SYSDEFS_H
#define SYSDEFS_H

#define DISABLE_INTERRUPTS \
   asm(".option arch, +zicsr\n\t" \
       "csrwi mstatus, 0");

#define ENABLE_INTERRUPTS \
   asm(".option arch, +zicsr\n\t" \
       "csrwi mstatus, 8");
#endif

// Timer definitions
#define  TIMER_TEN_MS         120000
#define  TIMER_HUNDRED_MS     1200000
#define  TIMER_QUARTER_SEC    3000000
#define  TIMER_ONE_SEC        12000000

#define  TIMER_RESET          1
#define  TIMER_ENABLE         2

