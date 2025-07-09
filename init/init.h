#ifndef INIT_H
#define INIT_H

#define BREAK asm("ebreak")

// Commands and scripts
int cli(int fd);
int parse_cmd(const char *raw_cmdbuf);
int run_script(const char *filename);

#endif
