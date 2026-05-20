/*
 * lab2_exec_example.c - Example demonstrating the exec() family of functions
 *
 * This program uses the exec() system call to learn how to replace
 * the current process image with a new program.
 *
 * Learning objectives:
 *   1. Understanding the types and naming conventions of exec() family functions
 *   2. Understanding why original code does not execute after a successful exec()
 *   3. Mastering the fork() + exec() + wait() pattern (how shells execute commands)
 *   4. Error handling when exec() fails
 *
 * Compile: gcc -Wall -o exec_example lab2_exec_example.c
 * Run:     ./exec_example
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <string.h>      /* String handling functions */
#include <unistd.h>      /* fork, exec family, getpid */
#include <sys/wait.h>    /* waitpid, WIFEXITED, WEXITSTATUS */

/*
 * The exec() function completely replaces the current process's code, data,
 * heap, stack, and all memory images with a new program.
 * If exec() succeeds, the code after it will never execute.
 * (Because the new program starts running from the beginning.)
 *
 * However, the PID does not change - the same process runs a different program.
 *
 * exec family naming convention (meaning of suffixes):
 *   l = list   : Arguments are passed one by one as variadic args (execl, execlp, execle)
 *   v = vector : Arguments are passed as a string array (char *argv[]) (execv, execvp, execvpe)
 *   p = PATH   : Searches for the executable in the PATH environment variable (execlp, execvp)
 *                Without p, the full path (e.g., /bin/ls) must be specified directly
 *   e = env    : Environment variables are specified directly (execle, execvpe)
 *                Without e, the current process's environment variables are inherited
 *
 * Key function signatures:
 *   execl(path, arg0, arg1, ..., NULL)
 *   execlp(file, arg0, arg1, ..., NULL)
 *   execv(path, argv[])
 *   execvp(file, argv[])
 */

/*
 * Helper function: After fork, runs a demo function in the child,
 * while the parent waits with wait.
 * This pattern ensures each demo runs independently, and protects
 * the entire program from ending if exec() succeeds.
 *
 * title: Demo title string
 * demo:  Function pointer to be called in the child process
 */
static void run_demo(const char *title, void (*demo)(void))
{
    printf("\n--- %s ---\n", title);

    /* fork() to create child - it is safest to always call exec() in a child */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        /* Child process: Run the demo function */
        demo();
        /*
         * If exec() succeeds, the code below will never be reached.
         * The code below executes only if exec() fails.
         */
        perror("exec failed");
        exit(1);
    } else {
        /*
         * Parent process: Uses waitpid() to wait for the specific child (pid) to terminate.
         * The third argument 0 to waitpid means blocking mode.
         * (Using WNOHANG would return immediately even if the child is still running)
         */
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("(Child terminated, exit code=%d)\n", WEXITSTATUS(status));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Demo 1: execl - Full path + argument list                           */
/* ------------------------------------------------------------------ */
static void demo_execl(void)
{
    printf("[Child PID=%d] Running /bin/echo with execl\n", getpid());
    /*
     * execl(path, arg0, arg1, ..., NULL)
     *
     * path: Full path to the program to execute ("/bin/echo")
     * arg0: By convention, the program name (corresponds to argv[0])
     * arg1~: Arguments to pass to the program
     * The argument list must end with (char *)NULL to mark the end.
     *
     * If this call succeeds, the current process is completely replaced
     * by /bin/echo, which prints "Hello from execl!" and exits.
     */
    execl("/bin/echo", "echo", "Hello", "from", "execl!", (char *)NULL);
    /* If execl succeeds, this line is never executed */
}

/* ------------------------------------------------------------------ */
/* Demo 2: execlp - PATH search + argument list                        */
/* ------------------------------------------------------------------ */
static void demo_execlp(void)
{
    printf("[Child PID=%d] Running ls with execlp (PATH search)\n", getpid());
    /*
     * execlp(file, arg0, arg1, ..., NULL)
     *
     * Similar to execl, but since it has 'p', it searches the PATH
     * environment variable for the program.
     * Therefore, you can specify just the name ("ls") instead of the
     * full path ("/bin/ls").
     * This has the same effect as typing "ls -l -h /tmp" in a shell.
     */
    execlp("ls", "ls", "-l", "-h", "/tmp", (char *)NULL);
}

/* ------------------------------------------------------------------ */
/* Demo 3: execv - Full path + argument array                          */
/* ------------------------------------------------------------------ */
static void demo_execv(void)
{
    printf("[Child PID=%d] Running /bin/echo with execv (using array)\n", getpid());
    /*
     * execv(path, argv[])
     *
     * Unlike execl, arguments are passed as a string array rather than
     * variadic arguments.
     * The argv array must end with NULL.
     *
     * The array approach is useful when the number of arguments is
     * determined at runtime.
     * (e.g., parsing user input and dynamically constructing arguments)
     */
    char *args[] = {"echo", "Hello", "from", "execv!", NULL};
    execv("/bin/echo", args);
}

/* ------------------------------------------------------------------ */
/* Demo 4: execvp - PATH search + argument array (most commonly used)  */
/* ------------------------------------------------------------------ */
static void demo_execvp(void)
{
    printf("[Child PID=%d] Running wc with execvp\n", getpid());
    /*
     * execvp(file, argv[])
     *
     * Combines PATH search (p) + array arguments (v).
     * This is the most commonly used combination in practice because:
     *   1. PATH search eliminates the need to know the full path.
     *   2. The array approach allows dynamic argument construction.
     *
     * When implementing a shell, execvp is typically used to execute
     * user-entered commands.
     */
    char *args[] = {"echo", "execvp works!", NULL};
    execvp("echo", args);
}

/* ------------------------------------------------------------------ */
/* Demo 5: fork + exec + wait = Standard pattern for running a new     */
/*         program                                                     */
/* ------------------------------------------------------------------ */
static void demo_pattern(void)
{
    /*
     * The standard pattern for running a new program in an OS:
     *   Step 1: fork()  - Create a child process
     *   Step 2: exec()  - Replace the child with a new program
     *   Step 3: wait()  - Parent waits for child termination
     *
     * This is the fundamental principle behind how Unix/Linux shells
     * execute commands.
     * Since this demo is already inside a fork+wait in run_demo(),
     * we only need to call exec here.
     */
    printf("[Child PID=%d] fork+exec pattern: Running date command\n", getpid());
    execlp("date", "date", (char *)NULL);
}

/* ------------------------------------------------------------------ */
/* Demo 6: Handling exec failure                                       */
/* ------------------------------------------------------------------ */
static void demo_exec_fail(void)
{
    printf("[Child PID=%d] Attempting to execute a nonexistent program\n", getpid());

    /*
     * Executing a nonexistent program with exec fails and returns -1.
     * Only when exec "fails" does the code after exec get executed.
     *
     * After failure, exit() must be called.
     * Otherwise, the child will continue executing the parent's remaining code,
     * causing unexpected behavior.
     *
     * By convention, exit code 127 is used for exec failure (shell convention).
     */
    execlp("this_program_does_not_exist",
           "this_program_does_not_exist", (char *)NULL);

    /* This line is reached only if exec fails */
    perror("exec failed (expected error)");
    exit(127);  /* 127: Conventional exit code meaning "command not found" */
}

int main(void)
{
    printf("=== exec() family demo ===\n");
    printf("Current process PID=%d\n", getpid());

    printf("\nKey concepts:\n");
    printf("  - exec() replaces the current process's code/data with a new program\n");
    printf("  - On exec() success, code after the call is never executed\n");
    printf("  - Therefore, exec() is usually called in a child after fork()\n");

    /* Run each demo wrapped in fork+wait, executed sequentially */
    run_demo("1. execl: Full path + argument list", demo_execl);
    run_demo("2. execlp: PATH search + argument list", demo_execlp);
    run_demo("3. execv: Full path + argument array", demo_execv);
    run_demo("4. execvp: PATH search + argument array (most commonly used)", demo_execvp);
    run_demo("5. fork+exec+wait pattern: Running date", demo_pattern);
    run_demo("6. Handling exec failure", demo_exec_fail);

    /* exec family summary table */
    printf("\n=== Summary ===\n");
    printf("  execl(path, arg0, ..., NULL)  - Direct path, argument list\n");
    printf("  execlp(file, arg0, ..., NULL) - PATH search, argument list\n");
    printf("  execv(path, argv[])           - Direct path, argument array\n");
    printf("  execvp(file, argv[])          - PATH search, argument array\n");
    printf("\nMost common shell pattern: fork() -> execvp() in child -> wait() in parent\n");

    return 0;
}
