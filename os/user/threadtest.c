#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/umutex.h"

#define STACK_SIZE 4096
#define N_THREADS  4

// ── Test 1: shared memory ────────────────────────────────────────────────────
// Two threads increment a shared counter using atomic add.
// Because they share the same page table (CLONE_VM equivalent),
// the final value must be exactly 2 * ITERS.

#define ITERS 100000

static volatile int shared_counter = 0;

static void counter_fn(void *arg)
{
  (void)arg;
  for(int i = 0; i < ITERS; i++)
    __sync_fetch_and_add(&shared_counter, 1);
  thread_exit();
}

static void test_shared_memory(void)
{
  char *stacks[2];
  int   tids[2];

  printf("test 1: shared memory ... ");

  shared_counter = 0;
  for(int i = 0; i < 2; i++){
    stacks[i] = malloc(STACK_SIZE);
    if(!stacks[i]){ printf("malloc failed\n"); exit(1); }
    tids[i] = thread_create(counter_fn, 0, stacks[i] + STACK_SIZE);
    if(tids[i] < 0){ printf("thread_create failed\n"); exit(1); }
  }
  for(int i = 0; i < 2; i++)
    thread_join(tids[i]);

  int expected = 2 * ITERS;
  if(shared_counter == expected)
    printf("OK (counter=%d)\n", shared_counter);
  else
    printf("FAIL (counter=%d expected %d — possible race w/o lock)\n",
           shared_counter, expected);

  for(int i = 0; i < 2; i++) free(stacks[i]);
}

// ── Test 2: many threads ──────────────────────────────────────────────────────
// Spawn N_THREADS threads each printing their id, then join them all.

static void hello_fn(void *arg)
{
  int id = (int)(uint64)arg;
  printf("  thread %d hello\n", id);
  thread_exit();
}

static void test_many_threads(void)
{
  char *stacks[N_THREADS];
  int   tids[N_THREADS];

  printf("test 2: %d threads ... \n", N_THREADS);

  for(int i = 0; i < N_THREADS; i++){
    stacks[i] = malloc(STACK_SIZE);
    if(!stacks[i]){ printf("malloc failed\n"); exit(1); }
    tids[i] = thread_create(hello_fn, (void*)(uint64)i, stacks[i] + STACK_SIZE);
    if(tids[i] < 0){ printf("thread_create failed\n"); exit(1); }
  }
  for(int i = 0; i < N_THREADS; i++)
    thread_join(tids[i]);

  printf("test 2: OK\n");
  for(int i = 0; i < N_THREADS; i++) free(stacks[i]);
}

// ── Test 3: mutex exclusion ───────────────────────────────────────────────────
// 4 threads each do 50000 increments of a shared counter protected by a mutex.
// Without the mutex the final value would be < 200000 due to lost updates.

static mutex_t g_mutex = MUTEX_INIT;
static int     g_counter = 0;

static void mutex_worker(void *arg)
{
  (void)arg;
  for(int i = 0; i < 50000; i++){
    mutex_lock(&g_mutex);
    g_counter++;
    mutex_unlock(&g_mutex);
  }
  thread_exit();
}

static void test_mutex(void)
{
  char *stacks[4];
  int   tids[4];

  printf("test 3: mutex exclusion ... ");
  g_counter = 0;

  for(int i = 0; i < 4; i++){
    stacks[i] = malloc(STACK_SIZE);
    tids[i]   = thread_create(mutex_worker, 0, stacks[i] + STACK_SIZE);
  }
  for(int i = 0; i < 4; i++)
    thread_join(tids[i]);

  if(g_counter == 200000)
    printf("OK (counter=%d)\n", g_counter);
  else
    printf("FAIL (counter=%d expected 200000)\n", g_counter);

  for(int i = 0; i < 4; i++) free(stacks[i]);
}

// ── Test 4: condvar producer / consumer ──────────────────────────────────────

#define QSIZE 8
static mutex_t   q_lock    = MUTEX_INIT;
static condvar_t q_notempty = CONDVAR_INIT;
static condvar_t q_notfull  = CONDVAR_INIT;
static int q_buf[QSIZE];
static int q_head = 0, q_tail = 0, q_count = 0;
static int prod_done = 0;

static void producer(void *arg)
{
  int n = *(int*)arg;
  for(int i = 0; i < n; i++){
    mutex_lock(&q_lock);
    while(q_count == QSIZE)
      condvar_wait(&q_notfull, &q_lock);
    q_buf[q_tail] = i;
    q_tail = (q_tail + 1) % QSIZE;
    q_count++;
    condvar_signal(&q_notempty);
    mutex_unlock(&q_lock);
  }
  mutex_lock(&q_lock);
  prod_done = 1;
  condvar_broadcast(&q_notempty);
  mutex_unlock(&q_lock);
  thread_exit();
}

static void consumer(void *arg)
{
  int *sum = (int*)arg;
  for(;;){
    mutex_lock(&q_lock);
    while(q_count == 0 && !prod_done)
      condvar_wait(&q_notempty, &q_lock);
    if(q_count == 0 && prod_done){
      mutex_unlock(&q_lock);
      break;
    }
    *sum += q_buf[q_head];
    q_head = (q_head + 1) % QSIZE;
    q_count--;
    condvar_signal(&q_notfull);
    mutex_unlock(&q_lock);
  }
  thread_exit();
}

static void test_condvar(void)
{
  printf("test 4: condvar producer/consumer ... ");

  int n = 1000, sum = 0;
  prod_done = 0; q_head = q_tail = q_count = 0;

  char *sp = malloc(STACK_SIZE), *sc = malloc(STACK_SIZE);
  int tp = thread_create(producer, &n, sp + STACK_SIZE);
  int tc = thread_create(consumer, &sum, sc + STACK_SIZE);
  thread_join(tp);
  thread_join(tc);

  int expected = n * (n - 1) / 2;
  if(sum == expected)
    printf("OK (sum=%d)\n", sum);
  else
    printf("FAIL (sum=%d expected %d)\n", sum, expected);

  free(sp); free(sc);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(void)
{
  printf("=== xv6 thread syscall test ===\n\n");
  test_shared_memory();
  test_many_threads();
  test_mutex();
  test_condvar();
  printf("\nAll tests done.\n");
  exit(0);
}
