/*
;The MIT License
;
;Copyright (c) 2025 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/

#include <stdint.h>
#include <poll.h>

#include "time.h"
#include "cpu.h"
#include "fd.h"

// -------------------------------------------------------------------
// Poll a list of file descriptors until at least one is ready or there
// is a timeout.
int 
SYS_poll(struct pollfd *fds, int nfds, int timeout)
{
   uint64_t current_time;
   uint64_t end_time;
   if(timeout) {
      current_time = get_ms();
      end_time = current_time + timeout;
   }

   int ready = 0;

   do {
      for(int i = 0; i < nfds; i++) {
         struct pollfd *ptr = &fds[i];
         if(ptr->fd < 0) continue;

         ptr->revents = 0;
         ssize_t bytes = SYS_peek(ptr->fd);

         if(bytes < 0) {
            ptr->revents = POLLERR;
            ready++;
         }
         else if(bytes > 0) {
            ptr->revents = POLLIN;
            ready++;
         }
      }

      if(ready) return ready;

   } while(timeout == 0 || get_ms() < end_time);

   return 0;
}
