#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "simulator.h"
#include "fd.h"

void
sim_init()
{
   fd_init();
}

void
device_log(const char *fmt, ...)
{
   printf("\e[36;1m[DEVICE] \e[0m");

   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
   printf("\n");
}

void
printk(const char *fmt, ...)
{
   printf("\e[33;1m[PRINTK] \e[0m");

   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
}

