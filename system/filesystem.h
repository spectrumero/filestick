#ifndef FILESYSTEM_H
#define FILESYSTEM_H
/*
;The MIT License
;
;Copyright (c) 2024 Dylan Smith
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
#include <stdint.h>
#include "ff.h"
#include <sys/dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
   bool  open;
   DIR   dir;
} DIRHND;

int SYS_mount(const char *src, const char *target, const char *fstype,
              unsigned long mountflags, const void *data);
int SYS_umount(const char *target);
int fatfs_to_errno(FRESULT res);

void init_dirs(void);

// System calls
int SYS_opendir(const char *path);
int SYS_closedir(int dh);
int SYS_readdir(int dh, struct dirent *d);

int SYS_unlink(const char *pathname);
int SYS_mkdir(const char *pathname, mode_t mode);
int SYS_stat(const char *pathname, struct stat *statbuf);

// File i/o on open files
void init_fileio(void);
int fileio_open(const char *path, int flags, mode_t mode);
ssize_t fileio_read(int fd, void *buf, size_t count);
ssize_t fileio_write(int fd, const void *buf, size_t count);
int fileio_close(int fd);
off_t fileio_lseek(int fd, off_t offset, int whence);
int fileio_fstat(int fd, struct stat *statbuf);

// Automount/mount at start
bool sd_insert_mount();

#endif

