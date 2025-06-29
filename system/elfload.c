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

#include <elf.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "console.h"
#include "fd.h"
#include "elfload.h"
#include "brk.h"
#include "printk.h"
#include "init.h"

// FIXME
#define USER_SP 0x20800

int elf_run(const char *args)
{
   uint8_t *user_sp = (uint8_t *)USER_SP;
   char *filename;
   int status;

   user_sp = setup_stack_args(args, user_sp, &filename);

   start_addr s = elf_load(filename, 0, &status);
   if(s) 
      init_user_with_sp(user_sp, s);
   else
      printk("Unable to run %s\n", filename);

   return status;
}

// MAXARGS should be multiples of 4 minus 1 to keep stack 16-byte
// aligned.
#define MAXARGS 7
//-------------------------------------------------------------------
// Sets up argc, argv on the userland stack.
void *setup_stack_args(const char *unparsed_args, void *sp, char **filename)
{
   uint8_t *stackptr = sp;
   uint32_t argc;
   char *args[MAXARGS];
   memset(args, 0, sizeof(args));
   
   uint16_t argbytes = strlen(unparsed_args) + 1;

   // 16 byte align
   if((argbytes & 0xFFF0) != argbytes) argbytes = (argbytes & 0xFFF0) + 16;
   stackptr -= argbytes;
   memcpy(stackptr, unparsed_args, strlen(unparsed_args));

   if(filename) filename[0] = stackptr;

   // tokenise the args on the user stack
   argc = 0;
   char *ptr = strtok(stackptr, " ");
   while(ptr && argc < MAXARGS) {
      args[argc] = ptr;
      argc++;

      ptr = strtok(NULL, " ");
   }

   // push the arg pointers onto the user stack
   stackptr -= sizeof(args);
   memcpy(stackptr, args, sizeof(args));

   // push argc onto the stack
   stackptr -= 4;
   memcpy(stackptr, &argc, 4);

   return stackptr;
}

//-------------------------------------------------------------------
// Loads the boot file from SPI flash. Returns the start address.
start_addr elf_boot() 
{
   // first try the root filesystem for a boot file
   int status;
   printk("trying !boot...\n");
   start_addr s = elf_load("/!boot", 0, &status);
   if(s)
      return s;
   else {
      printk("!boot not available, trying serial flash...\n");
      return elf_load("/dev/spiflash", FLASH_OFFSET, &status);
   }
}

//------------------------------------------------------------------
// Loads an ELF program, returning the start address
start_addr elf_load(const char *filename, uint32_t offset, int *status) 
{
   int fd;
   start_addr s;

   fd = SYS_open(filename, O_RDONLY, 0);
   if(fd < 0) {
      printk("error: Unable to open file, rc = %d\n", fd);
      return 0;
   }

   s = elf_load_fd(fd, offset, status);
   SYS_close(fd);
   return s;
}

//------------------------------------------------------------------
// Reads ELF headers and returns a start address if they are valid.
int elf_read_ehdr(int fd, uint32_t offset, Elf32_Ehdr *header)
{
   SYS_lseek(fd, offset, SEEK_SET);
   SYS_read(fd, header, sizeof(Elf32_Ehdr));
   uint32_t    *magic   = (uint32_t *)header;
   uint8_t     *class   = ((uint8_t *)header) + EI_CLASS;
   uint8_t     *eidata  = ((uint8_t *)header) + EI_DATA;

   if(*magic != ELF_MAGIC) {
      printk("Not an ELF file\n");
      return ENOEXEC;
   }

   if(*class != 1) {
      printk("Not a 32-bit executable\n");
      return ENOEXEC;
   }

   if(*eidata != 1) {
      printk("Not a little-endian data format\n");
      return ENOEXEC;
   }

   if(header->e_machine != EM_RISCV) {
      printk("Not a RISCV executable\n");
      return ENOEXEC;
   }

   if(header->e_type != ET_EXEC) {
      printk("Not an executable file\n");
      return ENOEXEC;
   }

   return 0;
}

//----------------------------------------------------------------
// Scans ELF phdr to ensure the executable is valid
int elf_validate_phdr(int fd, uint32_t offset, Elf32_Ehdr *header)
{
   Elf32_Phdr phdr;

   // Get the program headers and copy program data
   SYS_lseek(fd, offset + header->e_phoff, SEEK_SET);
   for(int i = 0; i < header->e_phnum; i++) {
      SYS_read(fd, &phdr, sizeof(Elf32_Phdr));

      if(phdr.p_type == PT_LOAD) {
         if(phdr.p_paddr < USRMEM_START) {
            printk("Invalid physical address: %x\n", phdr.p_paddr);
            return ENOEXEC;
         }
      } 
   }
   return 0;
}

//------------------------------------------------------------------
// Loads an ELF executable from the specified file descriptor.
// Returns a start address or 0 on failure.
// Status returned via *status
start_addr elf_load_fd(int fd, uint32_t offset, int *status) 
{
   Elf32_Ehdr header;
   Elf32_Phdr phdr;

   *status = elf_read_ehdr(fd, offset, &header);
   if(*status != 0) return 0;

   if((*status = elf_validate_phdr(fd, offset, &header)) == 0) {
      // Clear user memory
      memset((uint8_t *)USRMEM_START, 0, USRMEM_SIZE);

      SYS_lseek(fd, offset + header.e_phoff, SEEK_SET);
      for(int i = 0; i < header.e_phnum; i++) {
         SYS_read(fd, &phdr, sizeof(Elf32_Phdr));

         // record current position
         off_t curpos = SYS_lseek(fd, 0, SEEK_CUR);

         if(phdr.p_type == PT_LOAD) {
            if(phdr.p_paddr < USRMEM_START) {
               printk("Invalid physical address: %x\n", phdr.p_paddr);
               return 0;
            }

            SYS_lseek(fd, offset + phdr.p_offset, SEEK_SET);
            SYS_read(fd, (uint8_t *)phdr.p_paddr, phdr.p_filesz);
            SYS_lseek(fd, curpos, SEEK_SET);

            // set the program break
            set_min_brk((void *)(phdr.p_paddr + phdr.p_memsz));
         } 
      }
   }
   else {
      return 0;
   }

   return (start_addr)header.e_entry;
}

// Supervisor cmdlet
void super_elf(int argc, char **argv)
{
   if(!strcmp(argv[0], "boot"))
      init_user(elf_boot());
   else {
      if(argc != 2) 
         printk("usage: run <filename>\n");
      else 
         elf_run(argv[1]);
   }
}
