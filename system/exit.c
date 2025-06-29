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

#include <unistd.h>
#include <stdbool.h>

#include "elfload.h"
#include "exit.h"
#include "printk.h"
#include "super_shell.h"
#include "init.h"

//---------------------------------------------------------------
// Reload init from flash
void SYS_exit(int status) {
   int load_status;

   void *user_sp = setup_stack_args("init warm", (uint8_t *)USER_SP, NULL);
   start_addr s = elf_load("/dev/spiflash", FLASH_OFFSET, &load_status, user_sp);

   if(s) init_user_with_sp(user_sp, s);
   else {
      printk("unable to return to init, status = %d\n", load_status);
      while(1) super_shell();
   }
}

