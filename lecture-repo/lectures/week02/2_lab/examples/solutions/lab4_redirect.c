/*
 * lab4_redirect.c - I/O redirection example using dup2()
 *
 * This program uses file descriptors (fd) and the dup2() system call to learn
 * how to redirect standard I/O to files.
 * This is the principle behind how shells implement >, <, >> and other redirections.
 *
 * Learning objectives:
 *   1. The concept of file descriptors (fd) and standard fd numbers (0, 1, 2)
 *   2. How dup2(oldfd, newfd) works
 *   3. Implementing output redirection (cmd > file)
 *   4. Implementing input redirection (cmd < file)
 *   5. Implementing shell redirection with fork() + dup2() + exec() combination
 *
 * Compile: gcc -Wall -o redirect lab4_redirect.c
 * Run:     ./redirect
 */

#include <stdio.h>      /* printf, perror, fflush */
#include <stdlib.h>      /* exit */
#include <string.h>      /* strlen */
#include <unistd.h>      /* fork, dup, dup2, close, write, unlink */
#include <fcntl.h>       /* open, O_WRONLY, O_RDONLY, O_CREAT, O_TRUNC */
#include <sys/wait.h>    /* wait, WIFEXITED, WEXITSTATUS */

/*
 * File descriptor (fd) basic concepts:
 *   An integer number that refers to a file (or pipe, socket, etc.) opened by a process.
 *   Think of it as an index into the "file table" managed by the kernel.
 *
 *   Number  Name     Description
 *   ------  ------   -------------------------
 *   0       stdin    Standard input (keyboard)
 *   1       stdout   Standard output (screen)
 *   2       stderr   Standard error (screen)
 *   3+      (user)   Assigned when opened via open(), etc.
 *
 * dup2(oldfd, newfd) system call:
 *   Makes newfd point to the same file as oldfd.
 *   If newfd is already an open fd, it is closed first and then duplicated.
 *
 *   Example: dup2(fd, STDOUT_FILENO)
 *       --> fd 1 (stdout) now points to the file that fd points to.
 *       --> After this, output from printf() or write(1, ...) goes to that file.
 *
 * Redirection principle:
 *   "cmd > file"  = Connect stdout(1) to file   --> dup2(fd, 1)
 *   "cmd < file"  = Connect stdin(0) to file     --> dup2(fd, 0)
 *   "cmd >> file" = Append stdout(1) to file     --> open(..., O_APPEND) + dup2(fd, 1)
 */

/* ================================================================== */
/* Demo 1: Output redirection (stdout -> file)                         */
/* ================================================================== */
static void demo_output_redirect(void)
{
    printf("\n=== Demo 1: Output redirection (echo > output.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_output.txt";

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * Child process: Redirect stdout to a file and then execute echo.
         * This is how the shell handles "echo ... > output.txt".
         */

        /*
         * open(): Opens a file and returns a file descriptor (fd).
         *
         * Flag descriptions:
         *   O_WRONLY : Open for writing only
         *   O_CREAT  : Create the file if it does not exist
         *   O_TRUNC  : If the file already exists, truncate its contents (overwrite)
         * 0644: File permissions (owner read/write, group/others read)
         */
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        /*
         * dup2(fd, STDOUT_FILENO):
         *   Makes STDOUT_FILENO (=1, i.e., stdout) point to the file that fd points to.
         *   After this, all stdout output (printf, echo, etc.) goes to the file
         *   instead of the screen.
         */
        dup2(fd, STDOUT_FILENO);

        /*
         * Close the original fd.
         * Since stdout (1) already points to the same file via dup2,
         * the original fd is no longer needed. Not closing it causes an fd leak.
         */
        close(fd);

        /*
         * Execute echo with execlp.
         * echo writes to stdout, but since stdout has been replaced with a file,
         * the output is saved to /tmp/redirect_demo_output.txt instead of the screen.
         */
        execlp("echo", "echo", "This content is saved to a file!", (char *)NULL);
        perror("exec");
        exit(1);
    }

    /* Parent: Wait for child (echo) to finish */
    wait(NULL);

    /* Verify the saved content using cat */
    printf("[Parent] Contents of %s:\n", filename);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);
    printf("\n");

    /* Clean up temporary file - unlink() deletes a file */
    unlink(filename);
}

/* ================================================================== */
/* Demo 2: Input redirection (file -> stdin)                           */
/* ================================================================== */
static void demo_input_redirect(void)
{
    printf("\n=== Demo 2: Input redirection (wc < input.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_input.txt";

    /*
     * First, create a test input file.
     * Write 5 fruit names, one per line.
     */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    const char *content = "apple\nbanana\ncherry\ndate\nelderberry\n";
    write(fd, content, strlen(content));
    close(fd);

    printf("[Parent] Input file contents:\n%s\n", content);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * Child process: Redirect stdin to a file and then execute wc.
         * This is how the shell handles "wc -l < input.txt".
         */

        /* Open the input file in read-only mode (O_RDONLY) */
        int infd = open(filename, O_RDONLY);
        if (infd < 0) {
            perror("open");
            exit(1);
        }

        /*
         * dup2(infd, STDIN_FILENO):
         *   Makes STDIN_FILENO (=0, i.e., stdin) point to the file that infd points to.
         *   After this, when the program reads from stdin, it reads file contents
         *   instead of keyboard input.
         */
        dup2(infd, STDIN_FILENO);
        close(infd);  /* Close original fd */

        /*
         * Execute wc -l: Counts the number of lines from stdin.
         * Since stdin has been replaced with a file, it outputs the file's
         * line count (5).
         *
         * Note: printf uses stdout (fd 1), and only stdin was redirected,
         *       so printf output appears on the screen normally.
         */
        printf("[Child] wc -l result (line count): ");
        fflush(stdout);  /* Flush printf buffer immediately (must flush before exec) */
        execlp("wc", "wc", "-l", (char *)NULL);
        perror("exec");
        exit(1);
    }

    wait(NULL);

    /* Clean up temporary file */
    unlink(filename);
}

/* ================================================================== */
/* Demo 3: Understanding dup() basic behavior                          */
/* ================================================================== */
static void demo_dup_basics(void)
{
    printf("\n=== Demo 3: Understanding dup() basic behavior ===\n\n");

    const char *filename = "/tmp/redirect_demo_dup.txt";

    /* Open a file for writing */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }

    printf("Original fd = %d\n", fd);

    /*
     * dup(fd): Returns a new fd that points to the same file as fd.
     *
     * dup() assigns the lowest available fd number.
     * (0, 1, 2 are already used by stdin/stdout/stderr, so usually 3 or higher)
     *
     * Difference between dup() and dup2():
     *   dup(fd)           -> Kernel automatically assigns an available fd number
     *   dup2(fd, target)  -> Duplicates to the specified target number (used for redirection)
     *
     * The duplicated fd and the original fd share the same file and offset.
     */
    int new_fd = dup(fd);
    printf("dup(%d) = %d (points to the same file)\n", fd, new_fd);

    /*
     * Both fds can write to the same file.
     * Since they share the same offset, the second write continues
     * from where the first one left off.
     * (Data does not overlap or get overwritten)
     */
    const char *msg1 = "Written via original fd\n";
    const char *msg2 = "Written via duplicated fd\n";
    write(fd, msg1, strlen(msg1));      /* Write via original fd */
    write(new_fd, msg2, strlen(msg2));  /* Write via duplicated fd */

    /* Close both fds */
    close(fd);
    close(new_fd);

    /* Verify file contents with cat - both messages should be present */
    printf("\nFile contents:\n");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    unlink(filename);
}

/* ================================================================== */
/* Demo 4: Shell redirection implementation pattern                    */
/* ================================================================== */
static void demo_shell_redirect_pattern(void)
{
    printf("\n=== Demo 4: How the shell implements redirection ===\n\n");

    /*
     * The process of executing "sort < input > output" in a shell:
     *
     *   1. Create a child process with fork().
     *   2. In the child:
     *      a. Open the input file and dup2 it to stdin(0)    (input redirection)
     *      b. Open the output file and dup2 it to stdout(1)  (output redirection)
     *      c. Call exec("sort").
     *         Since sort reads from stdin and writes to stdout,
     *         it reads the input file and writes the sorted result to the output file.
     *   3. The parent calls wait() to wait for the child to terminate.
     *
     * Key point: dup2() must be called before exec().
     *            After exec(), the new program is running, so dup2 cannot be called.
     *            However, the fd table persists after exec(), so the
     *            redirected stdin/stdout remain in effect.
     */

    const char *infile = "/tmp/redirect_demo_sort_in.txt";
    const char *outfile = "/tmp/redirect_demo_sort_out.txt";

    /* Create the input file to be sorted (unsorted) */
    int fd = open(infile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const char *data = "cherry\napple\nbanana\ndate\n";
    write(fd, data, strlen(data));
    close(fd);

    printf("Input file (%s):\n%s\n", infile, data);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /*
         * Child process: Redirect both input and output, then execute sort.
         */

        /* --- Input redirection: stdin <- infile --- */
        int in_fd = open(infile, O_RDONLY);
        if (in_fd < 0) { perror("open input"); exit(1); }
        dup2(in_fd, STDIN_FILENO);   /* Replace stdin(0) with input file */
        close(in_fd);                /* Close original fd */

        /* --- Output redirection: stdout -> outfile --- */
        int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) { perror("open output"); exit(1); }
        dup2(out_fd, STDOUT_FILENO); /* Replace stdout(1) with output file */
        close(out_fd);               /* Close original fd */

        /*
         * Execute sort:
         *   sort reads data from stdin and outputs it in alphabetical order to stdout.
         *   However, since stdin and stdout have been replaced with files,
         *   it reads from infile and writes to outfile.
         */
        execlp("sort", "sort", (char *)NULL);
        perror("exec");
        exit(1);
    }

    /* Parent: Wait for sort to finish */
    wait(NULL);

    /* Verify the sorted output file contents */
    printf("Output file (%s):\n", outfile);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", outfile, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    /* Clean up temporary files */
    unlink(infile);
    unlink(outfile);
}

int main(void)
{
    printf("=== dup2() I/O redirection demo ===\n");
    printf("Key point: dup2(oldfd, newfd) -> newfd points to the same file as oldfd\n");

    demo_output_redirect();
    demo_input_redirect();
    demo_dup_basics();
    demo_shell_redirect_pattern();

    /* Shell redirection syntax and corresponding system calls summary */
    printf("\n=== Summary ===\n");
    printf("  > file  : fd = open(file, O_WRONLY|O_CREAT|O_TRUNC); dup2(fd, 1);\n");
    printf("  < file  : fd = open(file, O_RDONLY);                  dup2(fd, 0);\n");
    printf("  >> file : fd = open(file, O_WRONLY|O_CREAT|O_APPEND); dup2(fd, 1);\n");
    printf("\n  Shell pattern: fork() -> [child: redirect with dup2 -> exec()] -> [parent: wait()]\n");

    return 0;
}
