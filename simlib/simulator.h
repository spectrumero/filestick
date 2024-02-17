#ifndef SIMULATOR_H
#define SIMULATOR_H

#ifdef SIMULATOR
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

void sim_init(void);
void device_log(const char *fmt, ...);

// Simulated system call interface
ssize_t SIM_write(int fd, const void *buf, size_t count);
ssize_t SIM_read(int fd, void *buf, size_t count);
int SIM_fstat(int fd, struct stat *statbuf);
off_t SIM_lseek(int fd, off_t offset, int whence);
int SIM_close(int fd);
int SIM_open(const char *pathname, int flags, mode_t mode);

// for programs using the simulator, define syscalls to
// use the simlib wrapper.
#ifndef SIMLIB    // not building the library
#define write  SIM_write
#define read   SIM_read
#define fstat  SIM_fstat
#define lseek  SIM_lseek
#define close  SIM_close
#define open   SIM_open
#endif

#endif

#endif

