/* LINUX ONLY — reads /proc, runs dmesg. Not for xv6. */
#ifndef __linux__
#error "diag_collector.c is Linux-only. Not for xv6."
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "diag_collector.h"
#include "../thread/mutex.h"

/* ── utility ────────────────────────────────────────────────────────────── */

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int read_file(const char *path, char *buf, int buflen)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)(buflen - 1), f);
    if (n > 0) buf[n] = '\0'; else buf[0] = '\0';
    fclose(f);
    return n;
}

static int run_cmd(const char *cmd, char *buf, int buflen)
{
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)(buflen - 1), f);
    if (n > 0) buf[n] = '\0'; else buf[0] = '\0';
    pclose(f);
    return n;
}

/* ── per-source collector tasks ─────────────────────────────────────────── */
/*
 * Each collector is a small task function submitted to the worker pool.
 * They run in parallel, protected by the diag_collect_ctx mutex only when
 * writing back into the shared diag_result_t.
 */

typedef struct {
    diag_result_t *result;
    mutex_t       *result_lock;
} diag_ctx_t;

static void collect_meminfo(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[4096];
    if (read_file("/proc/meminfo", buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->meminfo, buf, sizeof(ctx->result->meminfo) - 1);
        ctx->result->collected |= DIAG_MEMINFO;
        mutex_unlock(ctx->result_lock);
    }
}

static void collect_cpu_stat(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[4096];
    if (read_file("/proc/stat", buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->cpu_stat, buf, sizeof(ctx->result->cpu_stat) - 1);
        ctx->result->collected |= DIAG_CPU_STAT;
        mutex_unlock(ctx->result_lock);
    }
}

static void collect_loadavg(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[256];
    if (read_file("/proc/loadavg", buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->loadavg, buf, sizeof(ctx->result->loadavg) - 1);
        ctx->result->collected |= DIAG_LOADAVG;
        mutex_unlock(ctx->result_lock);
    }
}

/*
 * Collect top processes by reading /proc/<pid>/stat for every numeric
 * directory entry.  This exercises directory traversal via the filesystem
 * layer and demonstrates IPC-free process introspection through /proc.
 */
static void collect_top_procs(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;

    typedef struct { long utime; long stime; char comm[64]; int pid; } proc_t;
    proc_t procs[512];
    int nprocs = 0;

    DIR *d = opendir("/proc");
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && nprocs < 512) {
        if (ent->d_name[0] < '1' || ent->d_name[0] > '9') continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/stat", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        int pid; char comm[64];
        char state; long utime, stime;
        /* Fields: pid comm state ppid pgroup session tty_nr tpgid flags
                   minflt cminflt majflt cmajflt utime stime */
        if (fscanf(f, "%d %63s %c %*d %*d %*d %*d %*d %*u "
                      "%*u %*u %*u %*u %ld %ld",
                   &pid, comm, &state, &utime, &stime) == 5) {
            procs[nprocs++] = (proc_t){
                .pid   = pid,
                .utime = utime,
                .stime = stime,
            };
            strncpy(procs[nprocs - 1].comm, comm, 63);
        }
        fclose(f);
    }
    closedir(d);

    /* Sort descending by total CPU ticks (utime + stime). */
    for (int i = 0; i < nprocs - 1; i++) {
        for (int j = i + 1; j < nprocs; j++) {
            if ((procs[j].utime + procs[j].stime) >
                (procs[i].utime + procs[i].stime)) {
                proc_t tmp = procs[i]; procs[i] = procs[j]; procs[j] = tmp;
            }
        }
    }

    char out[8192];
    int  off = 0;
    off += snprintf(out + off, sizeof(out) - (size_t)off,
                    "%-8s %-20s %10s\n", "PID", "COMM", "CPU_TICKS");
    int top = nprocs < 10 ? nprocs : 10;
    for (int i = 0; i < top; i++) {
        off += snprintf(out + off, sizeof(out) - (size_t)off,
                        "%-8d %-20s %10ld\n",
                        procs[i].pid, procs[i].comm,
                        procs[i].utime + procs[i].stime);
    }

    mutex_lock(ctx->result_lock);
    strncpy(ctx->result->top_procs, out, sizeof(ctx->result->top_procs) - 1);
    ctx->result->collected |= DIAG_TOP_PROCS;
    mutex_unlock(ctx->result_lock);
}

static void collect_dmesg(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[4096];
    /* tail -n 20: we only want recent kernel messages for the LLM prompt */
    if (run_cmd("dmesg --time-format=reltime 2>/dev/null | tail -n 20",
                buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->dmesg_tail, buf,
                sizeof(ctx->result->dmesg_tail) - 1);
        ctx->result->collected |= DIAG_DMESG;
        mutex_unlock(ctx->result_lock);
    }
}

static void collect_net(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[4096];
    if (read_file("/proc/net/dev", buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->net_stat, buf,
                sizeof(ctx->result->net_stat) - 1);
        ctx->result->collected |= DIAG_NET;
        mutex_unlock(ctx->result_lock);
    }
}

static void collect_disk(void *arg)
{
    diag_ctx_t *ctx = (diag_ctx_t *)arg;
    char buf[4096];
    if (read_file("/proc/diskstats", buf, sizeof(buf)) > 0) {
        mutex_lock(ctx->result_lock);
        strncpy(ctx->result->disk_stat, buf,
                sizeof(ctx->result->disk_stat) - 1);
        ctx->result->collected |= DIAG_DISK;
        mutex_unlock(ctx->result_lock);
    }
}

/* ── public API ─────────────────────────────────────────────────────────── */

int diag_collect(worker_pool_t *pool, diag_result_t *result, int parallel_jobs)
{
    (void)parallel_jobs;  /* pool already constrains parallelism via num_workers */

    memset(result, 0, sizeof(*result));

    mutex_t result_lock;
    mutex_init(&result_lock);

    /*
     * One shared context block per collection run.  All collector tasks point
     * at the same result and lock — they race to fill different fields, with
     * the mutex ensuring no two writers corrupt the same buffer.
     */
    diag_ctx_t ctx = { .result = result, .result_lock = &result_lock };

    long t0 = now_ms();

    /* Submit all collectors.  They run in parallel across the worker threads. */
    void (*collectors[])(void *) = {
        collect_meminfo, collect_cpu_stat, collect_loadavg,
        collect_top_procs, collect_dmesg, collect_net, collect_disk,
    };
    int n = (int)(sizeof(collectors) / sizeof(collectors[0]));

    for (int i = 0; i < n; i++)
        pool_submit(pool, collectors[i], &ctx, PRIORITY_NORMAL);

    /* Block until every task finishes — analogous to joining N threads. */
    pool_drain(pool);

    result->elapsed_ms = now_ms() - t0;
    mutex_destroy(&result_lock);
    return 0;
}

void diag_format_summary(const diag_result_t *result, char *buf, int buflen)
{
    int off = 0;

#define APPEND(label, field, flag) \
    if ((result->collected & (flag)) && off < buflen - 1) { \
        off += snprintf(buf + off, (size_t)(buflen - off), \
                        "=== " label " ===\n%s\n", result->field); \
    }

    APPEND("Memory",     meminfo,   DIAG_MEMINFO)
    APPEND("Load Avg",   loadavg,   DIAG_LOADAVG)
    APPEND("Top Procs",  top_procs, DIAG_TOP_PROCS)
    APPEND("CPU Stats",  cpu_stat,  DIAG_CPU_STAT)
    APPEND("dmesg",      dmesg_tail,DIAG_DMESG)
    APPEND("Network",    net_stat,  DIAG_NET)
    APPEND("Disk",       disk_stat, DIAG_DISK)

#undef APPEND

    if (off < buflen - 1)
        off += snprintf(buf + off, (size_t)(buflen - off),
                        "\n[collected in %ld ms]\n", result->elapsed_ms);
}
