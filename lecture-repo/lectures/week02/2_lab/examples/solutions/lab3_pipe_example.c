/*
 * lab3_pipe_example.c - Inter-process communication (IPC) example using pipe()
 *
 * This program uses the pipe() system call to learn how to send and receive
 * data between processes.
 *
 * Learning objectives:
 *   1. How to create a unidirectional communication channel with pipe()
 *   2. Why unused pipe ends (fd) must always be closed
 *   3. The pattern of using 2 pipes for bidirectional communication
 *   4. How combining dup2() and pipe implements the shell pipeline (cmd1 | cmd2)
 *
 * Compile: gcc -Wall -o pipe_example lab3_pipe_example.c
 * Run:     ./pipe_example
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <string.h>      /* strlen */
#include <unistd.h>      /* pipe, fork, read, write, close, dup2 */
#include <sys/wait.h>    /* wait, waitpid */

/*
 * pipe() creates a unidirectional communication channel (buffer) inside the kernel.
 *
 *   int fd[2];
 *   pipe(fd);
 *
 *   fd[0] = read end  - Data is read from this end
 *   fd[1] = write end - Data is written to this end
 *
 *   Data flow direction (unidirectional):
 *     Writer ---> write(fd[1]) ===[kernel buffer]=== read(fd[0]) ---> Reader
 *
 * Note: pipe() must be called before fork().
 *       After fork(), parent and child share the same fd[0] and fd[1],
 *       allowing them to communicate with each other.
 */

/* ================================================================== */
/* Demo 1: Parent -> Child unidirectional communication                */
/* ================================================================== */
static void demo_parent_to_child(void)
{
    printf("\n=== Demo 1: Parent -> Child unidirectional communication ===\n\n");

    int fd[2];  /* fd[0]=read, fd[1]=write */

    /*
     * pipe(fd): Creates a pipe.
     * Returns -1 on failure (e.g., system fd limit exceeded).
     */
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    /*
     * Since pipe() was called before fork(),
     * both parent and child have fd[0] and fd[1] after fork().
     * The parent writes to fd[1], and the child reads from fd[0].
     */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * Child process: Only reads from the pipe.
         *
         * Important: The unused write end (fd[1]) must be closed!
         * Reason: As long as the write end is open, read() will not receive EOF
         *         and will block forever expecting more data.
         *         All write ends must be closed for read() to return 0 (EOF).
         */
        close(fd[1]);  /* Child does not write, so close the write end */

        char buf[256];
        /*
         * read(fd[0], buf, size):
         *   Reads data from the pipe.
         *   If no data is available, it blocks until the writer calls write.
         *   Return value n: Number of bytes actually read (0 means EOF)
         */
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';  /* read() does not append a null terminator, so add it manually */
            printf("[Child] Message received from parent: \"%s\"\n", buf);
        }

        close(fd[0]);  /* Close the read end after reading */
        exit(0);
    } else {
        /*
         * Parent process: Only writes to the pipe.
         * Close the unused read end (fd[0]).
         */
        close(fd[0]);  /* Parent does not read, so close the read end */

        const char *msg = "Hello, child process!";
        printf("[Parent] Sending message to child: \"%s\"\n", msg);

        /* write(fd[1], msg, len): Writes data to the write end of the pipe */
        write(fd[1], msg, strlen(msg));

        /*
         * The write end must be closed so the child's read() can receive EOF (0).
         * If not closed, the child will assume "more data may still arrive"
         * and block forever in read(), causing the program to hang.
         */
        close(fd[1]);

        /* Wait for the child to terminate */
        wait(NULL);
    }
}

/* ================================================================== */
/* Demo 2: Bidirectional communication (using 2 pipes)                 */
/* ================================================================== */
static void demo_bidirectional(void)
{
    printf("\n=== Demo 2: Bidirectional communication (2 pipes) ===\n\n");

    /*
     * Since a pipe is unidirectional, two pipes are needed for bidirectional communication.
     *
     *   Parent ---write---> parent_to_child[1] ===pipe=== parent_to_child[0] ---read---> Child
     *   Parent ---read----> child_to_parent[0] ===pipe=== child_to_parent[1] ---write--> Child
     */
    int parent_to_child[2];  /* Parent -> Child direction pipe */
    int child_to_parent[2];  /* Child -> Parent direction pipe */

    if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * Child process:
         *   - Read from the parent_to_child pipe (only needs the read end)
         *   - Write to the child_to_parent pipe (only needs the write end)
         *   - Close all unused ends
         */
        close(parent_to_child[1]);  /* Write end of parent->child pipe: child does not write */
        close(child_to_parent[0]);  /* Read end of child->parent pipe: child does not read */

        /* Read message from parent */
        char buf[256];
        ssize_t n = read(parent_to_child[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[Child] Message received: \"%s\"\n", buf);
        }

        /* Send reply to parent */
        const char *reply = "Got it, thanks parent!";
        printf("[Child] Sending reply: \"%s\"\n", reply);
        write(child_to_parent[1], reply, strlen(reply));

        /* Close all used fds and exit */
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    } else {
        /*
         * Parent process:
         *   - Write to the parent_to_child pipe (only needs the write end)
         *   - Read from the child_to_parent pipe (only needs the read end)
         */
        close(parent_to_child[0]);  /* Read end of parent->child pipe: parent does not read */
        close(child_to_parent[1]);  /* Write end of child->parent pipe: parent does not write */

        /* Send message to child */
        const char *msg = "Hello, how are you?";
        printf("[Parent] Sending message: \"%s\"\n", msg);
        write(parent_to_child[1], msg, strlen(msg));
        close(parent_to_child[1]);  /* Close write end after sending to deliver EOF */

        /* Read reply from child */
        char buf[256];
        ssize_t n = read(child_to_parent[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[Parent] Reply from child: \"%s\"\n", buf);
        }

        close(child_to_parent[0]);
        wait(NULL);  /* Wait for child to terminate */
    }
}

/* ================================================================== */
/* Demo 3: Connecting two commands with pipe (shell's cmd1 | cmd2)     */
/* ================================================================== */
static void demo_pipe_commands(void)
{
    printf("\n=== Demo 3: Connecting commands with pipe (echo | wc) ===\n\n");

    /*
     * Same principle as running "echo hello world | wc -w" in a shell:
     *
     *   echo's stdout ----> fd[1] ===pipe=== fd[0] ----> wc's stdin
     *
     * That is, the standard output of the first command (echo) is connected
     * through the pipe to the standard input of the second command (wc).
     *
     * To achieve this, dup2() is used to:
     *   - Replace echo's stdout (fd 1) with the write end of the pipe
     *   - Replace wc's stdin (fd 0) with the read end of the pipe
     */

    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    /* ---- First child: echo (connect stdout to the pipe's write end) ---- */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid1 == 0) {
        /*
         * echo child: Redirect stdout to the pipe's write end (fd[1]).
         *
         * Steps:
         *   1. Close the read end (fd[0]) (echo does not read from the pipe)
         *   2. dup2(fd[1], STDOUT_FILENO) to replace stdout with the pipe write end
         *   3. Close the original fd[1] (already duplicated by dup2, original is unnecessary)
         *   4. Call exec("echo") - when echo writes to stdout, it goes into the pipe
         */
        close(fd[0]);                      /* 1. Close read end */
        dup2(fd[1], STDOUT_FILENO);        /* 2. stdout -> pipe write end */
        close(fd[1]);                      /* 3. Close original fd (prevent duplication) */

        /* Execute echo - output goes to the pipe instead of the screen */
        execlp("echo", "echo", "hello", "world", "from", "pipe", (char *)NULL);
        perror("exec echo");
        exit(1);
    }

    /* ---- Second child: wc (connect stdin to the pipe's read end) ---- */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        /*
         * wc child: Redirect stdin to the pipe's read end (fd[0]).
         *
         * Steps:
         *   1. Close the write end (fd[1]) (wc does not write to the pipe)
         *      Important: If this is not closed, wc's read() will never receive EOF!
         *   2. dup2(fd[0], STDIN_FILENO) to replace stdin with the pipe read end
         *   3. Close the original fd[0]
         *   4. Call exec("wc") - when wc reads from stdin, it reads the pipe data
         */
        close(fd[1]);                      /* 1. Close write end */
        dup2(fd[0], STDIN_FILENO);         /* 2. stdin -> pipe read end */
        close(fd[0]);                      /* 3. Close original fd */

        /* Execute wc -w - counts words (input comes from the pipe) */
        execlp("wc", "wc", "-w", (char *)NULL);
        perror("exec wc");
        exit(1);
    }

    /*
     * Parent process: Must close both ends of the pipe!
     *
     * If the parent does not close fd[1] (write end), the wc child will
     * block forever waiting for data from the pipe (since the parent could
     * still write to it).
     * If the parent does not close fd[0] (read end), an fd leak occurs.
     */
    close(fd[0]);
    close(fd[1]);

    /* Wait for both children to terminate */
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    printf("(The number above is the word count from echo's output)\n");
}

int main(void)
{
    printf("=== pipe() inter-process communication demo ===\n");
    printf("Key point: pipe(fd) -> fd[0]=read, fd[1]=write\n");

    demo_parent_to_child();
    demo_bidirectional();
    demo_pipe_commands();

    printf("\n=== Important notes summary ===\n");
    printf("1. Unused pipe fds must always be close()'d\n");
    printf("   - If the write end is open, the reader's read() will not receive EOF\n");
    printf("2. Pipes are unidirectional -> bidirectional communication needs 2 pipes\n");
    printf("3. If the pipe buffer is full, write() will block\n");

    return 0;
}
