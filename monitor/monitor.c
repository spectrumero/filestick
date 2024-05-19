#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/econet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "decoder.h"

// FIXME remove later
// Hardware registers
static volatile uint32_t *econet_state    = (uint32_t *)0x80011c;  // reg_status
static volatile uint8_t  *econet_mon      = (uint8_t  *)0x80011d;
static volatile uint32_t *tx_start_offset = (uint32_t *)0x800200;
static volatile uint32_t *tx_end_offset   = (uint32_t *)0x800204;
static volatile uint32_t *tx_flags        = (uint32_t *)0x800208;
static volatile uint32_t *timer_a_val     = (uint32_t *)0x800304;
static volatile uint32_t *timer_a_stat    = (uint32_t *)0x800308;
static volatile uint32_t *rx_start        = (uint32_t *)0x800100;
static volatile uint32_t *rx_end          = (uint32_t *)0x800104;
static volatile uint32_t *rx_sz           = (uint32_t *)0x800108;


static int econet_init(uint8_t station);
static uint8_t buf[2048];

int 
main(int argc, char **argv)
{
   printf("Econet monitor starting\n");

   int fd = econet_init(175);
   if(fd < 0) {
      printf("giving up!\n");
      return -1;
   }

   while(1) {
      ssize_t bytes = read(fd, buf, sizeof(buf));
      if(bytes < 0) {
         perror("read");
         return -1;
      }
      if(bytes > 5)
         decode_frame(buf, bytes);
      else
         printf("weird %d byte frame\n", bytes);
   }

   return 0;
}

// FIXME
volatile uint32_t *econet_hwctl = (uint32_t *)0x800320;

static int
econet_init(uint8_t station)
{
   int rc;
   int econet_fd;

   // FIXME
   //*econet_hwctl = 0x303;

   econet_fd = open("/dev/econet", O_RDWR);
   if(econet_fd < 0) {
      perror("econet open");
      return -1;
   }

   rc = ioctl(econet_fd, ECONET_SET_ADDR|station, NULL);
   if(rc < 0) {
      perror("setting station");
      return -1;
   }

   // test
   /*
   rc = ioctl(econet_fd, ECONET_SET_RECV_PORT|5, NULL);
   if(rc < 0) {
      perror("setting port");
      return -1;
   }*/
   /*
   rc = ioctl(econet_fd, ECONET_SET_MONITOR|1, NULL);
   if(rc < 0) {
      perror("setting monitor mode");
      return -1;
   }*/
   // FIXME!
   uint32_t *sr = (uint32_t *)0x80011c;
   *sr = 0xFFFFFFFF;
   printf("sr = %x\n", *sr);

   return econet_fd; 
}
