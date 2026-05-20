#ifndef DIAG_COLLECTOR_H
#define DIAG_COLLECTOR_H

#include "../worker/worker_pool.h"

/*
 * One snapshot of key OS-level metrics.
 * Each field is populated by a separate worker thread reading from /proc
 * or running a system utility — collection happens in parallel.
 */
typedef struct {
    char meminfo[4096];       /* /proc/meminfo  */
    char cpu_stat[4096];      /* /proc/stat (aggregate CPU ticks) */
    char loadavg[256];        /* /proc/loadavg  */
    char top_procs[8192];     /* top 10 processes by CPU (from /proc/<pid>/stat) */
    char dmesg_tail[4096];    /* last 20 lines of kernel ring buffer */
    char net_stat[4096];      /* /proc/net/dev  */
    char disk_stat[4096];     /* /proc/diskstats */
    int  collected;           /* bitmask — which fields were filled in */
    long elapsed_ms;          /* wall-clock time for the whole collection */
} diag_result_t;

/* Bitmask flags for diag_result_t.collected */
#define DIAG_MEMINFO   (1 << 0)
#define DIAG_CPU_STAT  (1 << 1)
#define DIAG_LOADAVG   (1 << 2)
#define DIAG_TOP_PROCS (1 << 3)
#define DIAG_DMESG     (1 << 4)
#define DIAG_NET       (1 << 5)
#define DIAG_DISK      (1 << 6)
#define DIAG_ALL       (0x7F)

/*
 * Collect all diagnostic sources in parallel using the supplied worker pool.
 * Blocks until every collector task finishes, then fills *result.
 *
 * parallel_jobs: how many sources to collect simultaneously (≤ pool workers).
 */
int diag_collect(worker_pool_t *pool, diag_result_t *result, int parallel_jobs);

/*
 * Render the result as a plain-text summary ready to be sent to the LLM.
 * buf must be at least DIAG_SUMMARY_LEN bytes.
 */
#define DIAG_SUMMARY_LEN (1024 * 32)
void diag_format_summary(const diag_result_t *result, char *buf, int buflen);

#endif /* DIAG_COLLECTOR_H */
