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

    /* TODO: Implement parent-to-child communication using a pipe.
     *
     * Steps:
     *   1. Declare int fd[2] and call pipe(fd). Check for errors (< 0).
     *
     *   2. Call fork(). Check for errors.
     *
     *   3. In the child process (pid == 0):
     *      a. Close the unused write end: close(fd[1])
     *         (Important: if the write end stays open, read() will never get EOF)
     *      b. Declare char buf[256]
     *      c. Call read(fd[0], buf, sizeof(buf) - 1) and store result in ssize_t n
     *      d. If n > 0, null-terminate buf (buf[n] = '\0') and print:
     *         "[Child] Message received from parent: \"%s\"\n"
     *      e. Close fd[0] and call exit(0)
     *
     *   4. In the parent process (else):
     *      a. Close the unused read end: close(fd[0])
     *      b. Set const char *msg = "Hello, child process!"
     *      c. Print: "[Parent] Sending message to child: \"%s\"\n"
     *      d. Call write(fd[1], msg, strlen(msg))
     *      e. Close fd[1] (so the child's read() receives EOF)
     *      f. Call wait(NULL) to wait for the child
     */
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

    /* TODO: Implement bidirectional communication using two pipes.
     *
     * Steps:
     *   1. Declare int parent_to_child[2] and int child_to_parent[2]
     *      Call pipe() on both. Check for errors.
     *
     *   2. Call fork(). Check for errors.
     *
     *   3. In the child process (pid == 0):
     *      a. Close unused ends:
     *         close(parent_to_child[1])  -- child does not write to parent->child pipe
     *         close(child_to_parent[0])  -- child does not read from child->parent pipe
     *      b. Read from parent_to_child[0] into char buf[256]
     *         If n > 0, null-terminate and print: "[Child] Message received: \"%s\"\n"
     *      c. Set const char *reply = "Got it, thanks parent!"
     *         Print: "[Child] Sending reply: \"%s\"\n"
     *         Write reply to child_to_parent[1]
     *      d. Close parent_to_child[0] and child_to_parent[1], then exit(0)
     *
     *   4. In the parent process (else):
     *      a. Close unused ends:
     *         close(parent_to_child[0])  -- parent does not read from parent->child pipe
     *         close(child_to_parent[1])  -- parent does not write to child->parent pipe
     *      b. Set const char *msg = "Hello, how are you?"
     *         Print: "[Parent] Sending message: \"%s\"\n"
     *         Write msg to parent_to_child[1]
     *         Close parent_to_child[1] (deliver EOF)
     *      c. Read reply from child_to_parent[0] into char buf[256]
     *         If n > 0, null-terminate and print: "[Parent] Reply from child: \"%s\"\n"
     *      d. Close child_to_parent[0] and call wait(NULL)
     */
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
        /* TODO: Redirect stdout to the pipe and exec echo.
         *
         * Steps:
         *   1. close(fd[0])                    -- echo does not read from pipe
         *   2. dup2(fd[1], STDOUT_FILENO)      -- stdout -> pipe write end
         *   3. close(fd[1])                    -- original fd no longer needed
         *   4. execlp("echo", "echo", "hello", "world", "from", "pipe", (char *)NULL)
         *   5. perror("exec echo") and exit(1) in case exec fails
         */
    }

    /* ---- Second child: wc (connect stdin to the pipe's read end) ---- */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        /* TODO: Redirect stdin to the pipe and exec wc.
         *
         * Steps:
         *   1. close(fd[1])                    -- wc does not write to pipe
         *      Important: if not closed, wc's read() will never receive EOF!
         *   2. dup2(fd[0], STDIN_FILENO)       -- stdin -> pipe read end
         *   3. close(fd[0])                    -- original fd no longer needed
         *   4. execlp("wc", "wc", "-w", (char *)NULL)
         *   5. perror("exec wc") and exit(1) in case exec fails
         */
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
