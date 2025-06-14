#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <syscall.h>
#include <string.h>

// lengths include CRC
#define SCOUT_LEN    8
#define IMMSCOUT_LEN 10
#define ACK_LEN      6
#define BCAST_LEN    16

#define IMM_PEEK     0x81
#define IMM_POKE     0x82
#define IMM_JSR      0x83
#define IMM_USERPROC 0x84
#define IMM_OSPROC   0x85
#define IMM_HALT     0x86
#define IMM_CONT     0x87
#define IMM_MTYPE    0x88
#define IMM_GETREG   0x89
#define FIRST_IMM    0x81
#define LAST_IMM     0x89

char *imm_ops[] = {
   "PEEK", "POKE", "JSR", "UserProc", "OsProc", "Halt", "Cont", "MachType",
   "GetReg" };

static uint8_t lastack[4];
static uint16_t ack_count;

static void decode_address(uint8_t *buf);
static void decode_scout(uint8_t *buf);
static void decode_immscout(uint8_t *buf);
static void decode_data(uint8_t *buf, size_t length);
static void decode_bcast(uint8_t *buf, size_t length);

void
decode_frame(uint8_t *buf, size_t length)
{
   // deal with any repeat acks first
   if(length != ACK_LEN && ack_count) {
      if(ack_count > 1) {
         decode_address(lastack);
         printf("%d acks received\n", ack_count);
      }
      ack_count = 0;
      memset(lastack, 0, sizeof(lastack));
   }

   switch(length) {
      case SCOUT_LEN:
         decode_address(buf);
         decode_scout(buf + 4);
         break;
      case IMMSCOUT_LEN:
         decode_address(buf);
         decode_immscout(buf + 4);
         break;
      case ACK_LEN:
         if(!memcmp(lastack, buf, 4)) ack_count++;

         else {
            decode_address(buf);
            printf("ack %02x %02x\n", buf[4], buf[5]);
            memcpy(lastack, buf, 4);
            ack_count = 0;
         }
         break;
      default:
         decode_address(buf);
         // dst_stn = buf[0]
         if(buf[0] == 0)
            decode_bcast(buf + 4, length - 4);
         else
            decode_data(buf + 4, length - 4);
   }
}

static void
decode_address(uint8_t *buf)
{
   // print the source and destination
   uint8_t src_net = buf[3];
   uint8_t src_stn = buf[2];
   uint8_t dst_net = buf[1];
   uint8_t dst_stn = buf[0];
   printf("%03d.%03d > %03d.%03d\t", src_net, src_stn, dst_net, dst_stn);

}

static void
decode_scout(uint8_t *buf)
{
   printf("SCOUT: ctl %02x port %02x\n", buf[0], buf[1]);
}

static void
decode_immscout(uint8_t *buf)
{
   char *type = (*buf >= FIRST_IMM && * buf <= LAST_IMM) ?
      imm_ops[*buf] : "Unknown";
   printf("IMM: %02x %s\n", *buf, type);
}

static void
decode_data(uint8_t *buf, size_t length)
{
   printf("DATA: len %d\n", length);

   int dumplen = length > 256 ? 256 : length;
   hexdump(buf, dumplen, 0);
}

static void
decode_bcast(uint8_t *buf, size_t length)
{
   printf("BCAST: ctl %02x port %02x ", buf[0], buf[1]);
}

