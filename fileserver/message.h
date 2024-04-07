#ifndef MESSAGE_H
#define MESSAGE_H
#include <stdint.h>

#define NETFS_PORT   0x99

typedef enum {
   FC_COMMANDLINE = 0,     // 0
   FC_SAVE,                // 1
   FC_LOAD,                // 2
   FC_EXAMINE,             // 3
   FC_CATHEADER,           // 4
   FC_LOADCOMMAND,         // 5
   FC_OPEN,                // 6
   FC_CLOSE,               // 7
   FC_GETBYTE,             // 8
   FC_PUTBYTE,             // 9
   FC_GETBYTES,            // 0x0a  10 
   FC_PUTBYTES,            // 0x0b  11
   FC_READ_RACCESS_INFO,   // 0x0c  12
   FC_SET_RACCESS_INFO,    // 0x0d  13
   FC_READ_DISCNAME,       // 0x0e  14
   FC_WHO,                 // 0x0f  15
   FC_READ_DATETIME,       // 0x10  16
   FC_READ_EOF,            // 0x11  17
   FC_READ_OBJINFO,        // 0x12  18
   FC_SET_OBJINFO,         // 0x13  19
   FC_DEL_OBJECT,          // 0x14  20
   FC_READ_USERENV,        // 0x15  21
   FC_SET_BOOTOPT,         // 0x16  22
   FC_LOGOFF,              // 0x17  23
   FC_READ_USERINFO,       // 0x18  24
   FC_READ_FSVERSION,      // 0x19  25
   FC_READ_FSFREESPACE,    // 0x1a  26
   FC_MKDIR,               // 0x1b  27
   FC_SET_DATETIME,        // 0x1c  28
   FC_CREATEFILE,          // 0x1d  29
   FC_ACORN_READ_FREESPACE, // 0x1e 30
   FC_ACORN_SET_FREESPACE, // 0x1f  31
   FC_READ_CLIENT_USERID   // 0x20  32
} FunctionCode;


typedef struct {
   uint8_t        reply_net;
   uint8_t        reply_station;
   uint8_t        reply_port;
   FunctionCode   function_code;
   uint8_t        urd;           // User Root Directory handle
   uint8_t        csd;           // Current directory handle
   uint8_t        lib;           // Library directory handle
   uint16_t       paysize;       // Payload size
   uint8_t        *payload;
} NetFSMsg;

int   econet_init(uint8_t station);
void  econet_msgloop();
void  econet_send(NetFSMsg *origin, uint8_t *msg, size_t msgsize);

#endif
