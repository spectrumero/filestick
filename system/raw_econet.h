#ifndef RAW_ECONET_H
#define RAW_ECONET_H
/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
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

#include <stdlib.h>
#include <stdint.h>

#define ECONET_TXBUFSZ     512
#define ECONET_RXBUFSZ     512

// Hardware driver states
#define STATE_WAITSCOUT    0           // idle
#define STATE_WAITDATA     1           // waiting for data frame to us
#define STATE_TXSCOUT      2           // sending TX scout frame
#define STATE_TXDATA       3           // sending TX data frame

// TX status from the ISR
#define STATUS_TXDONE      1           // Successful transmission
#define STATUS_TXNETERR    2           // Generic failure of transmission

void econet_init();
int econet_open(const char *devname, int flags, mode_t mode, FD *fd);

// ioctl requests and structs are defined in sys/econet.h
int econet_ioctl(int fd, unsigned long request, void *ptr);
ssize_t econet_read(int fd, void *ptr, size_t count);
ssize_t econet_write(int fd, const void *ptr, size_t count);
int econet_close(int fd);

#endif

