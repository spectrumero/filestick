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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "console.h"
#include "fd.h"
#include "elfload.h"
#include "brk.h"

//-------------------------------------------------------------------
// Loads the boot file from SPI flash. Returns the start address.
uint32_t elf_boot() {
   int fd;
   uint32_t start_addr;

   // Clear user memory
   memset((uint8_t *)USRMEM_START, 0, USRMEM_SIZE);

   fd = SYS_open("/dev/spiflash", O_RDONLY, 0);
   if(fd < 0) {
      kerr_puts("Unable to open SPI flash");
      return 0;
   }

   start_addr = elf_load(fd, FLASH_OFFSET);
   SYS_close(fd);
   return start_addr;
}

//------------------------------------------------------------------
// Loads an ELF executable from the specified file descriptor.
// Returns a start address or 0 on failure.
uint32_t elf_load(int fd, uint32_t offset) {
   Elf32_Ehdr header;
   Elf32_Phdr phdr;
   ssize_t bytes;

   SYS_lseek(fd, offset, SEEK_SET);
   SYS_read(fd, &header, sizeof(Elf32_Ehdr));
   uint32_t    *magic   = (uint32_t *)&header;
   uint8_t     *class   = ((uint8_t *)&header) + EI_CLASS;
   uint8_t     *eidata  = ((uint8_t *)&header) + EI_DATA;

   if(*magic != ELF_MAGIC) {
      kerr_puts("Not an ELF file");
      return 0;
   }

   if(*class != 1) {
      kerr_puts("Not a 32-bit executable");
      return 0;
   }

   if(*eidata != 1) {
      kerr_puts("Not a little-endian data format");
      return 0;
   }

   if(header.e_machine != EM_RISCV) {
      kerr_puts("Not a RISCV executable");
      return 0;
   }

   if(header.e_type != ET_EXEC) {
      kerr_puts("Not an executable file");
      return 0;
   }

   // Get the program headers and copy program data
   SYS_lseek(fd, offset + header.e_phoff, SEEK_SET);
   for(int i = 0; i < header.e_phnum; i++) {
      SYS_read(fd, &phdr, sizeof(Elf32_Phdr));

      // record current position
      off_t curpos = SYS_lseek(fd, 0, SEEK_CUR);

      if(phdr.p_type == PT_LOAD) {
         if(phdr.p_paddr < USRMEM_START) {
            kerr_puts("Invalid physical address");
            return 0;
         }

         SYS_lseek(fd, offset + phdr.p_offset, SEEK_SET);
         SYS_read(fd, (uint8_t *)phdr.p_paddr, phdr.p_filesz);
         SYS_lseek(fd, curpos, SEEK_SET);

         // set the program break
         set_min_brk((void *)(phdr.p_paddr + phdr.p_memsz));
      } 
   }

   return header.e_entry;
}

