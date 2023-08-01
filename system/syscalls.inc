# Assembler file for system call numbers

.set SYSCALL_lowest, 17
.set SYSCALL_hi_lowest, 1024

.data
syscall_table:
.byte 0           # 17 SYS_getcwd
.byte 0           # 18
.byte 0           # 19
.byte 0           # 20
.byte 0           # 21
.byte 0           # 22
.byte 0           # 23 SYS_dup
.byte 0           # 24
.byte 0           # 25 SYS_fcntl
.byte 0           # 26
.byte 0           # 27
.byte 0           # 28
.byte 0           # 29
.byte 0           # 30
.byte 0           # 31
.byte 8           # 32 console_hexdump  -- (nonstd)
.byte 9           # 33 console_hexword  -- (nonstd)
.byte 0           # 34
.byte 0           # 35
.byte 0           # 36
.byte 0           # 37
.byte 0           # 38
.byte 0           # 39
.byte 0           # 40
.byte 0           # 41
.byte 0           # 42
.byte 0           # 43
.byte 0           # 44
.byte 0           # 45
.byte 0           # 46
.byte 0           # 47
.byte 0           # 48 SYS_faccessat
.byte 0           # 49 SYS_chdir
.byte 0           # 50
.byte 0           # 51
.byte 0           # 52
.byte 0           # 53
.byte 0           # 54
.byte 0           # 55
.byte 0           # 56 SYS_openat
.byte 4           # 57 SYS_close
.byte 0           # 58 
.byte 0           # 59
.byte 0           # 60
.byte 0           # 61 SYS_getdents
.byte 6           # 62 SYS_lseek
.byte 2           # 63 SYS_read
.byte 1           # 64 SYS_write
.byte 0           # 65 
.byte 0           # 66 SYS_writev
.byte 0           # 67 SYS_pread
.byte 0           # 68 SYS_pwrite
.byte 0           # 69
.byte 0           # 70 
.byte 0           # 71
.byte 0           # 72
.byte 0           # 73
.byte 0           # 74
.byte 0           # 75
.byte 0           # 76
.byte 0           # 77
.byte 0           # 78
.byte 0           # 79 SYS_fstatat
.byte 5           # 80 SYS_fstat
.byte 0           # 81
.byte 0           # 82
.byte 0           # 83
.byte 0           # 84
.byte 0           # 85
.byte 0           # 86
.byte 0           # 87
.byte 0           # 88
.byte 0           # 89
.byte 0           # 90
.byte 0           # 91
.byte 0           # 92
.byte 7           # 93 SYS_exit
.byte 0           # 94
.set syscall_table_sz, .-syscall_table

syscall_high_table:
.byte 3           # 1024 SYS_open
.byte 0           # 1025 SYS_link
.byte 0           # 1026 SYS_unlink
.byte 0           # 1027
.byte 0           # 1028
.byte 0           # 1029
.byte 0           # 1030 SYS_mkdir
.byte 0           # 1031
.byte 0           # 1032
.byte 0           # 1033 SYS_access
.byte 0           # 1034
.byte 0           # 1035
.byte 0           # 1036
.byte 0           # 1037
.byte 0           # 1038 SYS_stat
.byte 0           # 1039 SYS_lstat
.byte 0
.set syscall_high_table_sz, .-syscall_high_table

.align 4
syscall_address:
.word 0x00000000  # dummy entry
.word SYS_write   # 1
.word SYS_read    # 2
.word SYS_open    # 3
.word SYS_close   # 4
.word SYS_fstat   # 5
.word SYS_lseek   # 6
.word SYS_exit    # 7
.word console_hexdump # 8
.word console_hexword # 9

