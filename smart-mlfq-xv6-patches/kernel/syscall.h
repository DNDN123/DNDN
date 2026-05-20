// Smart-MLFQ-xv6 modified syscall.h
// =================================
// Added: SYS_setpri (30), SYS_getstats (31), SYS_settrace (32), SYS_forkpri (33)
//
// Team-wide syscall number allocation (W13 integration plan):
//   22..24  reserved for Syscall slice (minju): trace_on/off/stats
//   25..29  reserved for Thread   slice (jinhwan): thread_*/futex_*
//   30..33  Scheduler slice (hyunsung): this block
// Stay out of 22..29 so other slices can merge without renumbering.

// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_pause  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_setpri   30   // Smart-MLFQ
#define SYS_getstats 31   // Smart-MLFQ
#define SYS_settrace 32   // Smart-MLFQ (I2)
#define SYS_forkpri  33   // Smart-MLFQ (I4)
