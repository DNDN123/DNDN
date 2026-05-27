// Integrated syscall.h
// ========================
// 22-24: minju (trace)
// 25-29: jinhwan (thread/futex)
// 30-33: hyunsung (Smart-MLFQ)
// 34+ : haneol (process/ps)

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
// --- minju (trace) ---
#define SYS_trace_on    22
#define SYS_trace_off   23
#define SYS_trace_stats 24
// --- jinhwan (thread/futex) ---
#define SYS_thread_create 25
#define SYS_thread_join   26
#define SYS_thread_exit   27
#define SYS_futex_wait    28
#define SYS_futex_wake    29
// --- hyunsung (Smart-MLFQ) ---
#define SYS_setpri   30
#define SYS_getstats 31
#define SYS_settrace 32
#define SYS_forkpri  33
// --- haneol (process) ---
#define SYS_ps       34
#define SYS_sysinfo  35
