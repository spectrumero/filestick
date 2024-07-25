#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

int main(int argc, char **argv) {
   int binfd, hexfd;
   struct stat st;
   char hexbuf[10];

   if(argc < 3) {
      fprintf(stderr, "usage: bin2vhex <infile> <outfile>\n");
      return -1;
   }

   if(stat(argv[1], &st) < 0) {
      perror("stat");
      return -1;
   }

   if(st.st_size & 3 != 0) {
      fprintf(stderr, "File size must be exactly divisible by 4\n");
      return -1;
   }

   binfd = open(argv[1], O_RDONLY);
   if(binfd < 0) {
      perror("open: read");
      return -1;
   }

   hexfd = open(argv[2], O_CREAT|O_WRONLY, 0644);
   if(hexfd < 0) {
      perror("open: write");
      return -1;
   }

   uint8_t *bindata = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, binfd, 0);
   if(bindata == MAP_FAILED) {
      perror("mmap");
      return -1;
   }

   uint8_t *binptr = bindata;
   for(int i = 0; i < st.st_size; i += 4) {
      sprintf(hexbuf, "%02x%02x%02x%02x ", *binptr++, *binptr++, *binptr++, *binptr++);
      write(hexfd, hexbuf, strlen(hexbuf));
   }
   write(hexfd, "\n", 1);

   close(hexfd);
   close(binfd);
   munmap(bindata, st.st_size);
   return 0;
}
