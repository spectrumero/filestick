#ifndef UDP_ECONET_H
#define UDP_ECONET_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_STATIONS    64
#define MAX_ECONET_BUF  2048     // maximum econet datagram size

enum {
   WAIT_SCOUT,
   WAIT_DATA
} EconetRxState;

typedef struct {
   uint8_t           net;
   uint8_t           station;
   struct sockaddr_in addr;
   uint16_t          port;
} EconetStation;

// Configure and start up simulated econet.
// Returns 0 on success.
int sim_config_econet(const char *cfgfile, uint8_t our_net, uint8_t our_station);

#endif
