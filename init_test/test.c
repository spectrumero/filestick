#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void cons_print(const char *str);
void run_tests() {
   char buf[256];

   cons_print("Filestick test program\r\n");
   cons_print("Type some text> ");

   size_t bytes=read(0, buf, sizeof(buf));
   cons_print("\r\nRead: ");
   write(1, buf, bytes);
   cons_print("\r\n");

   exit(0);
}

void cons_print(const char *str) {
   write(1, str, strlen(str));
}
