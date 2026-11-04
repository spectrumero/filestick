# FileStick system calls from BASIC

The `ECALL` function makes a system call directly:

```
result% = ECALL(n [, a0 [, a1 ... [, a6]]])
```

n goes in a7 and the up to seven arguments in a0 to a6. Registers
without an argument are 0. ECALL returns a0, the system call's result.

The system call table is in `system/syscalls.inc`, the dispatcher in
`system/super_trap.s`.

## Conventions

**Return values.** Most calls return 0 or a positive value on success
and a negative error number on failure. ECALL returns what the system
returns, so failure shows up as a negative value. There is no `errno`.
An unknown call number returns -2000.

**Strings.** System calls take C strings, which end with a zero byte.
`$addr = "..."` ends the string with &0D, as on the BBC Micro, so add a
zero yourself:

```
DIM name% 64
$name% = "data.txt" + CHR$0
```

**Buffers and structures.** Use memory reserved with `DIM buf% size`,
which is always word aligned. Read and write fields with the indirection
operators: `!` for 32 bit words, `?` for bytes and `$` for strings.
For example, `buf%!16` is the word at offset 16. To read a C string that
the system wrote, collect characters up to the zero byte:

```
DEF FNcstr(a%):LOCAL s$:IF ?a%=0 THEN =""
REPEAT s$=s$+CHR$?a%:a%=a%+1:UNTIL ?a%=0:=s$
```

**File descriptors.** 0 is stdin, 1 is stdout and 2 is stderr (all
the console). BASIC's channel numbers (from OPENIN etc.) are file
descriptors, so `BGET#`, `PTR#`, `CLOSE#` etc. work on files opened with
ECALL, and ECALL works on channels opened from BASIC.

## Error numbers

| Value | Name | Meaning |
|---|---|---|
| -2 | ENOENT | no such file, directory or device |
| -5 | EIO | disk error |
| -9 | EBADF | bad file descriptor or directory handle |
| -12 | ENOMEM | out of memory |
| -17 | EEXIST | file exists |
| -22 | EINVAL | invalid argument |
| -23 | ENFILE | no free directory handles |
| -24 | EMFILE | no free file descriptors |
| -29 | ESPIPE | can't seek on this device |
| -30 | EROFS | write protected |
| -2000 | | no such system call |
| -2001 - n | | other FatFS error n (FRESULT in `system/ff.h`) |

Common FatFS errors: -2004 drive not ready, -2007 invalid name,
-2008 access denied or directory full, -2012 invalid drive,
-2014 no FAT filesystem.

## System calls

| n | Name | Arguments | Returns |
|---|---|---|---|
| 18 | opendir | path | directory handle |
| 19 | closedir | handle | 0 |
| 20 | readdir | handle, dirent | 0 |
| 21 | peek | fd | bytes waiting to be read |
| 22 | printk | format, up to 6 values | (nothing) |
| 24 | exec | command line | only returns on failure |
| 29 | ioctl | fd, request, pointer | 0 |
| 32 | hexdump | address, length, displayed address | (nothing) |
| 33 | supervisor shell | | |
| 34 | malloc_init | memory, size | allocator handle, or 0 |
| 35 | malloc | allocator, size | address, or 0 |
| 36 | realloc | allocator, address, size | address, or 0 |
| 37 | free | allocator, address | (nothing) |
| 39 | umount | drive | 0 |
| 40 | mount | source, target, fstype, flags, data | 0 |
| 49 | chdir | path | 0 |
| 57 | close | fd | 0 |
| 62 | lseek | fd, offset, whence | new position |
| 63 | read | fd, buffer, count | bytes read |
| 64 | write | fd, buffer, count | bytes written |
| 75 | poll | pollfds, count, timeout | number of ready fds |
| 80 | fstat | fd, stat | 0 |
| 93 | exit | status | doesn't return |
| 214 | brk | address | program break |
| 1024 | open | path, flags, mode | fd |
| 1026 | unlink | path | 0 |
| 1030 | mkdir | path, mode | 0 |
| 1038 | stat | path, stat | 0 |

Any other number returns -2000. The numbers are Linux's RISC-V ones
where there is a Linux equivalent; 1024 and above are from the old
Linux generic table. The rest are FileStick only.

### Files

**open (1024)** `fd% = ECALL(1024, path%, flags%, 0)`

The mode argument is ignored. Flags, added together:

| Flag | Value | |
|---|---|---|
| O_RDONLY | 0 | read |
| O_WRONLY | 1 | write |
| O_RDWR | 2 | read and write |
| O_APPEND | &8 | open at the end of the file |
| O_CREAT | &200 | create the file |
| O_TRUNC | &400 | empty the file |

O_CREAT always empties an existing file (it is the same as O_TRUNC);
use O_APPEND without O_CREAT to add to an existing file. Paths are FatFS
paths: `0:` is the flash and `1:` the SD card, e.g. `1:/dir/file`.
Paths starting with `/dev/` open a device instead: `/dev/console`,
`/dev/rgbled`, `/dev/spiflash` or `/dev/econet`.

**read (63)** `n% = ECALL(63, fd%, buf%, count%)`
reads up to count% bytes into buf%. Returns 0 at the end of the file.
Reading the console (fd 0) waits for a line.

**write (64)** `n% = ECALL(64, fd%, buf%, count%)`
writes count% bytes from buf%.

**lseek (62)** `pos% = ECALL(62, fd%, offset%, whence%)`
whence% is 0 (from the start), 1 (from the current position) or 2 (from
the end). `ECALL(62, fd%, 0, 2)` returns the length of a file. Returns
-29 for the console.

**close (57)** `r% = ECALL(57, fd%)`
Closing fds 0 to 2 does nothing.

**peek (21)** `n% = ECALL(21, fd%)`
returns the number of bytes that can be read without waiting. Works on
the console and Econet.

**fstat (80)** `r% = ECALL(80, fd%, st%)`
and **stat (1038)** `r% = ECALL(1038, path%, st%)` fill in an 88 byte
`struct stat` (reserve it with `DIM st% 87`). The useful fields are:

| Offset | Size | Field | |
|---|---|---|---|
| 4 | 4 | st_mode | &4000 directory, &8000 file, plus &100 readable and &80 writable |
| 8 | 2 | st_nlink | |
| 16 | 4 | st_size | size in bytes |

fstat on a file sets only st_mode (&8000) and st_size. (Systems built
before the `memset` fix of November 2026 write these two fields 87 bytes
too far on, past the end of the structure.)

**poll (75)** `n% = ECALL(75, fds%, count%, timeout%)`
waits until at least one of count% file descriptors has data to read,
or timeout% milliseconds pass. **A timeout of 0 waits forever.** fds%
points to count% 8 byte entries:

| Offset | Size | Field | |
|---|---|---|---|
| 0 | 4 | fd | the fd to watch, or -1 to skip the entry |
| 4 | 2 | events | ignored |
| 6 | 2 | revents | set to 1 (data waiting) or 4 (error), else 0 |

Returns the number of entries with revents set, or 0 on timeout.

**ioctl (29)** `r% = ECALL(29, fd%, request%, ptr%)`
controls a device. The console (`include/sys/console.h`) takes:

| Request | |
|---|---|
| &02000000 | raw mode: characters are read one at a time, without echo |
| &03000000 | interactive mode: lines are read with echo and editing |
| &01000000 + flags | set the control flags directly (1 raw, 2 XON/XOFF flow control) |
| &10000000 | discard typeahead |

BASIC sets the console mode itself, so changing it can upset INPUT and
GET. Econet requests are in `include/sys/econet.h`; for example,
`ECONET_GET_ADDR` (&81000000) stores the station number in the byte at
ptr%.

### Directories and filesystems

**opendir (18)** `dh% = ECALL(18, path%)`
opens a directory and returns a handle (0 to 7). Only 8 can be open at
once. **readdir (20)** `r% = ECALL(20, dh%, d%)` reads the next entry
into a 264 byte `struct dirent` (`DIM d% 263`):

| Offset | Size | Field | |
|---|---|---|---|
| 0 | 256 | d_name | name, zero terminated; empty at the end of the directory |
| 256 | 1 | d_isdir | non-zero for a directory |
| 260 | 4 | d_size | size in bytes |

**closedir (19)** `r% = ECALL(19, dh%)` closes the handle.

**mkdir (1030)** `r% = ECALL(1030, path%, 0)`. The mode is ignored.

**unlink (1026)** `r% = ECALL(1026, path%)` deletes a file or empty
directory.

**chdir (49)** `r% = ECALL(49, path%)`

**mount (40)** `r% = ECALL(40, src%, 0, fstype%, 0, 0)`
mounts a FAT filesystem. src% is the string `sd` or `flash`, and
fstype% must be `fatfs`. The target, flags and data are ignored (pass
0): the SD card is always drive 1 and the flash drive 0.

**umount (39)** `r% = ECALL(39, drive%)` where drive% is the string `0`
(flash) or `1` (SD card).

### Programs

**exec (24)** `r% = ECALL(24, cmd%)`
replaces BASIC with another program. cmd% is the program's file name
followed by up to 6 space separated arguments, e.g. `monitor.elf foo`.
It only returns (with an error number) if the program couldn't be
loaded.

**exit (93)** `ECALL(93, status%)` ends BASIC and reloads init. A
non-zero status prints "Abnormal exit". Unlike `*BYE` this doesn't
restore the console settings.

**brk (214)** `b% = ECALL(214, addr%)`
sets the program break, and returns the new break or, if addr% is out
of range, the current one. `ECALL(214, 0)` just reads it. BASIC's
workspace and the C library's malloc pool are below the break, so don't
lower it.

### Memory allocation

The system has a TLSF allocator that works on a block of memory you
give it, such as one reserved with DIM.

**malloc_init (34)** `h% = ECALL(34, mem%, size%)` sets up an
allocator in size% bytes at mem% (which must be word aligned) and returns
a handle, or 0 if the block is too small. The allocator's own data takes
a few KB of the block.

**malloc (35)** `p% = ECALL(35, h%, size%)`,
**realloc (36)** `p% = ECALL(36, h%, p%, size%)` and
**free (37)** `ECALL(37, h%, p%)` work as in C. They return 0 if there
isn't enough memory.

### Debugging

**printk (22)** `ECALL(22, fmt%, v1, v2 ...)` prints to the console
with a C style format string. Up to 6 values can follow the format
(they go in a1 to a6). It supports `%d %u %x %X %c %s %p %%` and field
widths (e.g. `%08x`); `%s` takes the address of a zero terminated
string. A line feed (CHR$10) is printed as CR LF. It writes directly to the serial port,
so BASIC's COUNT and POS don't see the output.

**hexdump (32)** `ECALL(32, addr%, len%, shown%)` prints len% bytes
from addr% as a hex dump, numbering the lines from shown% (usually the
same as addr%).

**supervisor shell (33)** enters the system's debug shell, which has
the prompt `S>`.

## Examples

Print a message (take the length from the string you stored, since
reading `$m%` back stops at the first &0D):

```
10 DIM m% 40 : s$ = "Hello" + CHR$13 + CHR$10
20 $m% = s$ : n% = ECALL(64, 1, m%, LEN(s$))
```

printk with values:

```
10 DIM f% 40 : $f% = "A%%=%d, hex %08x" + CHR$10 + CHR$0
20 A% = 1234
30 x% = ECALL(22, f%, A%, A%)
```

Write a file and get its size:

```
10 DIM p% 40, b% 40
20 $p% = "test.txt" + CHR$0
30 fd% = ECALL(1024, p%, &201, 0) : REM O_WRONLY + O_CREAT
40 IF fd% < 0 THEN PRINT "open failed: "; fd% : END
50 s$ = "Some text" : $b% = s$
60 n% = ECALL(64, fd%, b%, LEN(s$))
70 r% = ECALL(57, fd%)
80 DIM st% 87
90 r% = ECALL(1038, p%, st%)
100 PRINT "size "; st%!16
```

List a directory:

```
10 DIM p% 40, d% 263
20 $p% = "/" + CHR$0
30 dh% = ECALL(18, p%)
40 IF dh% < 0 THEN PRINT "opendir failed: "; dh% : END
50 REPEAT
60   r% = ECALL(20, dh%, d%)
70   IF r% = 0 AND ?d% <> 0 THEN PRINT FNcstr(d%), d%!260, d%?256 <> 0
80 UNTIL r% <> 0 OR ?d% = 0
90 r% = ECALL(19, dh%)
100 END
110 DEF FNcstr(a%):LOCAL s$:IF ?a%=0 THEN =""
120 REPEAT s$=s$+CHR$?a%:a%=a%+1:UNTIL ?a%=0:=s$
```

Wait up to 5 seconds for a key:

```
10 DIM fds% 7
20 fds%!0 = 0 : fds%!4 = 0
30 n% = ECALL(75, fds%, 1, 5000)
40 IF n% THEN PRINT "key: "; GET$ ELSE PRINT "timeout"
```
