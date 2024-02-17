#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>

#include "udp_econet.h"

static int sockfd;      // our UDP socket fd
static EconetStation stations[MAX_STATIONS];
static EconetStation *our = NULL;
static pthread_t econet_thread;

// create the UDP socket
static int
sim_udp_init()
{
   struct sockaddr_in servaddr;   

   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if(sockfd < 0) return sockfd;

   memset(&servaddr, 0, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = our->addr.sin_addr.s_addr;
   servaddr.sin_port = htons(our->port);

   return 0;
}

// Read the econet configuration info in b-em format
// (net, station, ip, port)
static int
parse_stn(char *buf, EconetStation *stn)
{
   static struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_DGRAM,
      .ai_flags = 0,
      .ai_protocol = 0};

   struct addrinfo *result;
   char host[128];
   uint8_t net, station;
   uint16_t port;

   sscanf(buf, "%d %d %s %d\n", net, station, host, port);
   if(station > 0 && strlen(host) > 0 && port > 0) {
      int rc = getaddrinfo(host, "", &hints, &result);
      if(rc != 0) {
         fprintf(stderr, "unable to look up host %s\n", host);
         return 0;
      } 
      
      if(result->ai_family = AF_INET) {
         struct sockaddr_in *sin = (struct sockaddr_in *)result->ai_addr;
         memcpy(&stn->addr, sin, sizeof(struct sockaddr_in));

         stn->net = net;
         stn->station = station;
         stn->port = port;

         freeaddrinfo(result);
         return 1;
      }

      freeaddrinfo(result);
   }

   // not a valid config line
   return 0;
}

// wait a defined amount of time for a message. Returns the ready fd or -1 if
// timed out waiting.
int
sim_waitformsg()
{
   fd_set rfds;
   FD_ZERO(&rfds);
   FD_SET(sockfd, &rfds);
   struct timeval tv;

   tv.tv_sec = 1;
   tv.tv_usec = 0;

   int rc=select(1, &rfds, NULL, NULL, &tv);
   if(rc < 0) return rc;

   if(FD_ISSET(0, &rfds)) return sockfd;

   return 0;
}

static int
handle_scout(uint8_t *rxbuf, size_t size)
{
   return WAIT_SCOUT;
}

static int
handle_data(uint8_t *rxbuf, size_t size)
{
   return WAIT_SCOUT;
}

// Main econet listening thread.
static void *
sim_econet_thread(void *ptr)
{
   socklen_t len;
   int rxbytes;
   int state = WAIT_SCOUT;
   struct sockaddr_in cliaddr;
   uint8_t rxbuf[MAX_ECONET_BUF];

   while(1) {
      len = sizeof(cliaddr);
     
      switch(state) {
         case WAIT_SCOUT: 
            rxbytes = recvfrom(sockfd, (uint8_t *)rxbuf, sizeof(rxbuf), 0,
               (struct sockaddr *)&cliaddr, &len);
            state = handle_scout(rxbuf, len);
            break;
         case WAIT_DATA:
            if(sim_waitformsg() > 0) {
               rxbytes = recvfrom(sockfd, (uint8_t *)rxbuf, sizeof(rxbuf), 0,
                  (struct sockaddr *)&cliaddr, &len);
               handle_data(rxbuf, len); 
            }
            state = WAIT_SCOUT;
            break;
         default:
            fprintf(stderr, "sim_econet_thread: unexpected state: %d\n", state);
            state = WAIT_SCOUT;
      }
   }
}

// Start the econet listening thread.
static int
sim_start_econet()
{
   int rc = pthread_create(&econet_thread, NULL, sim_econet_thread, NULL);
   if(rc != 0) return rc;

   return 0;
}

// Configure and start up simulated econet.
int
sim_config_econet(const char *cfgfile, uint8_t our_net, uint8_t our_station)
{
   char buf[128];
   int idx = 0;

   FILE *stream = fopen(cfgfile, "r");
   if(!stream) return -1;

   memset(stations, 0, sizeof(stations));

   while(fgets(buf, sizeof(buf), stream)) {
      if(isdigit(buf[0])) {
         EconetStation *stn = &stations[idx];
         idx += parse_stn(buf, stn);

         if(stn->net == our_net && stn->station == our_station)
            our = stn;
      }
   }

   fclose(stream);

   // no configuration for our station?
   if(our = NULL) {
      errno = EADDRNOTAVAIL;
      return -1;
   }

   if(sim_udp_init() < 0) return -1;

   return sim_start_econet();
}

