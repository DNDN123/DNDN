// Integrated user.h
// =================================
// Added: setpri/getstats/settrace/forkpri (hyunsung)
//      + struct procstats (hyunsung)
//      + trace_on/off/stats + struct trace_stats (minju)
//      + thread_create/join/exit + futex_wait/wake (jinhwan)
//      + ps + sysinfo (haneol)
// Include guard added because jinhwan/umutex.h re-includes user.h.

#ifndef XV6_USER_H
#define XV6_USER_H

#define SBRK_ERROR ((char *)-1)

struct stat;

// === Smart-MLFQ: stats struct returned by getstats ===
struct procstats {
  int pid;
  int priority;
  int total_run_ticks;
  int total_io_blocks;
  int arrival_tick;
  int completion_tick;
  int final_pri;
};

// === minju: 시스템콜 추적 모듈 ===
// 커널 struct proc 의 추적 필드와 같은 레이아웃이어야 함.
// K2 fix: array size 32 → 64 to cover all integrated syscalls (up to 35).
struct trace_stats {
  int    traced;
  uint64 syscall_count[64];
  uint64 syscall_errors[64];
};

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
int setpri(int pid, int level);                // Smart-MLFQ
int getstats(int pid, struct procstats *out);  // Smart-MLFQ
int settrace(int pid, int on);                 // Smart-MLFQ (I2)
int forkpri(int level);                        // Smart-MLFQ (I4)
// === minju: 시스템콜 추적 ===
int trace_on(void);
int trace_off(void);
int trace_stats(struct trace_stats *st);

// === jinhwan: thread / futex ===
int thread_create(void (*fcn)(void *), void *arg, void *stack);
int thread_join(int tid);
void thread_exit(void) __attribute__((noreturn));
int futex_wait(volatile int *addr, int expected);
int futex_wake(volatile int *addr);

// === haneol: process listing / sysinfo ===
struct procinfo;   // forward decl; full def in kernel/procinfo.h
struct sysinfo;    // forward decl; full def in kernel/sysinfo.h
int ps(struct procinfo *buf, int max);
int sysinfo(struct sysinfo *info);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char* sbrk(int);
char* sbrklazy(int);

// printf.c
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));

// umalloc.c
void* malloc(uint);
void free(void*);

#endif /* XV6_USER_H */
