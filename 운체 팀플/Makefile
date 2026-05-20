CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g \
           -I src \
           -D_GNU_SOURCE

# All source files
THREAD_SRCS = src/thread/thread.c \
              src/thread/mutex.c  \
              src/thread/condvar.c

WORKER_SRCS = src/worker/worker_pool.c

DIAG_SRCS   = src/diag/diag_collector.c

LIB_SRCS    = $(THREAD_SRCS) $(WORKER_SRCS) $(DIAG_SRCS)
LIB_OBJS    = $(LIB_SRCS:.c=.o)

TEST_SRC    = test/test_thread.c
TEST_BIN    = test_thread

.PHONY: all clean test

all: $(TEST_BIN)

$(TEST_BIN): $(LIB_OBJS) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(LIB_OBJS) $(TEST_BIN)
