#
#The MIT License
#
#Copyright (c) 2023 Dylan Smith
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.
#

.option arch, +zicsr

.data
initstr:
.ascii "Loading initial program\r\n"
.set initstr_sz, .-initstr
fake_boot_file:
.asciz "init cold"

#define USER_SP      0x20800     # end of RAM

.text
.globl init
init:
   la    sp, __stack_top         # initialize system stack

   call  fd_init                 # initialize file descriptors
   call  init_dirs               # initialize directory handles
   call  init_fileio             # initialize file io

   la    a0, __cons_buff         # clear down the console buffer
   mv    a1, zero
   li    a2, 256
   call  memset
   
   la    t0, super_trap          # initialize the supervisor (ecall) trap
   csrw  stvec, t0

   call  kmalloc_init            # initialize TLSF memory allocator for kmalloc

   call  econet_init             # initialize econet

   li    a0, 1
   la    a1, initstr
   li    a2, initstr_sz
   li    a7, 64
   call  syscall_handler

   call  flash_reset
   call  sd_insert_mount         # Mount SD card if present

   call  elf_boot                # load boot code from flash

   la    t0, isr_trap            # Interrupt routine address
   csrw  mtvec, t0
   csrwi mstatus, 8              # enable interrupts

   beqz  a0, .badboot            # boot file not OK?

.globl init_user
init_user:
   mv    s0, a0                  # preserve start address
   la    a0, fake_boot_file      # pointer to the fake boot filename
   li    a1, USER_SP             # The user stack pointer
   li    a2, 0                   # null
   call  setup_stack_args
   mv    sp, a0                  # set up stack pointer
   jr    s0                      # start initial program

.globl init_user_with_sp
init_user_with_sp:
   mv    sp, a0                  # sp passed as arg1
   jr    a1                      # start address as arg2

.badboot:
   call  super_shell
   j     .badboot

