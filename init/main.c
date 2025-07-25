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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/econet.h>
#include <sys/ioctl.h>
#include <sys/dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/mount.h>

#include "init.h"

// ----------------------------------------------------------------------
// On startup, argv[1] if present indicates whether this is a cold
// or warm start, or potentially any other useful information.
int
main(int argc, char **argv)
{
   if(argc > 1) {
      if(!strcmp(argv[1], "cold")) {
         printf("Cold boot, initializing flash file system\n");
         int rc = mount("flash", "", "fatfs", 0, NULL);
         if(rc < 0) {
            int e = errno;
            perror("mount");
            printf("errno = %d\n", e);
         }

         if(run_script("boot.rc") == -1) {
            printf("No boot.rc file\n");
         }
      }
   }

   // Run CLI on stdin
   cli(0);
   return 0;
}

