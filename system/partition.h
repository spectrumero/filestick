#ifndef _PARTITION_H
#define _PARTITION_H
#include <stdint.h>

typedef struct _pte {
   uint8_t status;
   uint8_t starthead;
   uint8_t startsec;
   uint8_t startcyl;
   uint8_t type;
   uint8_t endhead;
   uint8_t endsec;
   uint8_t endcyl;
   uint32_t startlba;
   uint32_t sectors;
} PTEntry;

#endif

