#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

void hexdump(void *buf, size_t count); // actually a syscall
void hexword(uint32_t word);
void cons_print(const char *str);
void flashtest();

void run_tests() {
   char buf[256];

   cons_print("Filestick test program\r\n");
   cons_print("Type some text> ");

   size_t bytes=read(0, buf, sizeof(buf));
   cons_print("\r\nRead: ");
   write(1, buf, bytes);
   cons_print("\r\n");

   flashtest();

   exit(0);
}

void flashtest() {
   uint8_t buf[256];
   int fd = open("/dev/spiflash", O_RDONLY);
   if(fd < 0) {
      cons_print("Unable to open spiflash: ");
      hexword(fd);
      cons_print("\r\n");
      return;
   }

   lseek(fd, 0x30000, SEEK_SET);
   read(fd, buf, sizeof(buf));
   close(fd);

   hexdump(buf, sizeof(buf));
}

void cons_print(const char *str) {
   write(1, str, strlen(str));
}

