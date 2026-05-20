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

        /* TODO: Implement output redirection and exec echo.
         *
         * Steps:
         *   1. Open the file for writing:
         *      int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)
         *      - O_WRONLY: write only
         *      - O_CREAT: create if it doesn't exist
         *      - O_TRUNC: truncate (overwrite) if it exists
         *      - 0644: permissions (owner rw, group/others r)
         *      If fd < 0, perror("open") and exit(1)
         *
         *   2. dup2(fd, STDOUT_FILENO)
         *      This makes stdout point to the opened file.
         *      After this, all stdout output goes to the file.
         *
         *   3. close(fd)
         *      The original fd is no longer needed since stdout already
         *      points to the file. Not closing causes an fd leak.
         *
         *   4. execlp("echo", "echo", "This content is saved to a file!", (char *)NULL)
         *      echo writes to stdout, which now goes to the file.
         *
         *   5. perror("exec") and exit(1) in case exec fails.
         */
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

        /* TODO: Implement input redirection and exec wc.
         *
         * Steps:
         *   1. Open the input file in read-only mode:
         *      int infd = open(filename, O_RDONLY)
         *      If infd < 0, perror("open") and exit(1)
         *
         *   2. dup2(infd, STDIN_FILENO)
         *      This makes stdin point to the file.
         *      After this, reading from stdin reads file contents.
         *
         *   3. close(infd) -- original fd no longer needed
         *
         *   4. Print "[Child] wc -l result (line count): " and call fflush(stdout)
         *      (Must flush printf buffer before exec replaces the process)
         *
         *   5. execlp("wc", "wc", "-l", (char *)NULL)
         *      wc reads from stdin, which now reads from the file.
         *
         *   6. perror("exec") and exit(1) in case exec fails.
         */
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

    /* TODO: Use dup() and write to both fds.
     *
     * Steps:
     *   1. int new_fd = dup(fd)
     *      Print: "dup(%d) = %d (points to the same file)\n" with fd and new_fd
     *
     *   2. Write via both fds to show they share the same file:
     *      const char *msg1 = "Written via original fd\n";
     *      const char *msg2 = "Written via duplicated fd\n";
     *      write(fd, msg1, strlen(msg1));
     *      write(new_fd, msg2, strlen(msg2));
     *      (They share the same offset, so msg2 appears after msg1)
     *
     *   3. Close both: close(fd) and close(new_fd)
     */

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

        /* TODO: Implement "sort < infile > outfile" using dup2.
         *
         * Steps:
         *   1. Input redirection (stdin <- infile):
         *      int in_fd = open(infile, O_RDONLY)
         *      if (in_fd < 0) { perror("open input"); exit(1); }
         *      dup2(in_fd, STDIN_FILENO)   -- replace stdin with input file
         *      close(in_fd)                -- close original fd
         *
         *   2. Output redirection (stdout -> outfile):
         *      int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644)
         *      if (out_fd < 0) { perror("open output"); exit(1); }
         *      dup2(out_fd, STDOUT_FILENO) -- replace stdout with output file
         *      close(out_fd)               -- close original fd
         *
         *   3. Execute sort:
         *      execlp("sort", "sort", (char *)NULL)
         *      perror("exec") and exit(1) in case of failure
         */
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
