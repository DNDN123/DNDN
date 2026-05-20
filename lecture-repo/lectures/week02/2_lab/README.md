# Week 2 Lab: Process 1 - fork, exec, wait, pipe, I/O

> Textbook: xv6 Ch 1 - Operating System Interfaces
> Duration: approximately 50 minutes
> Environment: Linux or macOS (gcc required)

---

## Learning Objectives

Through this lab, you will understand:

- How to duplicate a process with `fork()` and wait for a child with `wait()`
- How to replace the current process with a new program using the `exec()` family
- How to send and receive data between processes using `pipe()`
- How to redirect standard I/O to files or pipes using `dup2()`

The combination of these four system calls is the core principle behind the Unix shell.

---

## Prerequisites

```bash
# Verify that gcc is installed
gcc --version

# Navigate to the lab directory
cd examples/
```

> **xv6 Environment Setup**: Starting from Week 3, you will need QEMU and the RISC-V toolchain to build and run xv6. See **[setup_xv6_env.md](setup_xv6_env.md)** for installation instructions (macOS / Ubuntu / Windows WSL2).

---

## Lab 1: fork() and wait() Basics (~12 min)

### Background

`fork()` duplicates the current process to create a child process.

```
Before fork():          After fork():

  [Parent PID=100]      [Parent PID=100]  fork() returns = child PID (200)
                          |
                          +-- [Child PID=200]  fork() returns = 0
```

- Parent process: `fork()` returns the child's PID
- Child process: `fork()` returns 0
- On failure: returns -1

`wait()` blocks (suspends) the parent until the child process terminates.

### Lab Steps

**Step 1**: Compile and run the example code.

```bash
gcc -Wall -o lab1_fork_basic lab1_fork_basic.c
./lab1_fork_basic
```

**Step 2**: Observe the output and answer the following questions.

- Q1: Who prints first, the parent or the child? Is it the same every time you run it?
- Q2: What happens if you remove `wait()`? (Comment it out and recompile)
- Q3: When the child exits with `exit(42)`, how can the parent retrieve the value 42?

**Step 3**: Try modifying it yourself

Modify `lab1_fork_basic.c` so that 5 child processes are created. Have each child exit with a different exit code (0~4), and have the parent print all the exit codes.

### Key Summary

```c
pid_t pid = fork();
if (pid == 0) {
    // Child code
    exit(0);
} else {
    // Parent code
    int status;
    wait(&status);            // Wait until child terminates
    WEXITSTATUS(status);      // Extract child's exit code
}
```

---

## Lab 2: The exec() Family (~12 min)

### Background

`exec()` **completely replaces** the current process's code, data, and stack with a new program. If it succeeds, it does not return.

```
Before exec():              After exec():
[Process PID=200]           [Process PID=200]
 Code: fork_basic            Code: /bin/ls        <-- completely replaced
 Data: ...                   Data: ...
 PID stays the same!        PID stays the same!
```

Naming convention for the exec family functions:

| Suffix | Meaning | Example |
|--------|---------|---------|
| `l` | Arguments as a list | `execl("/bin/ls", "ls", "-l", NULL)` |
| `v` | Arguments as a vector (array) | `execv("/bin/ls", argv)` |
| `p` | Search PATH environment variable | `execlp("ls", "ls", "-l", NULL)` |
| `e` | Specify environment variables directly | `execle(path, arg, ..., envp)` |

### Lab Steps

**Step 1**: Compile and run the example code.

```bash
gcc -Wall -o lab2_exec_example lab2_exec_example.c
./lab2_exec_example
```

**Step 2**: Observe the output and answer the following questions.

- Q1: Does the `printf()` after the `exec()` call get executed? Why or why not?
- Q2: In Demo 6, why does `perror` print when exec fails?
- Q3: What happens if you call `exec()` without `fork()`?

**Step 3**: Try modifying it yourself

Add a new demo to `lab2_exec_example.c`: use `execvp` to run `grep` and search for a pattern in a specific file.

```c
// Hint: Run "grep" "pattern" "filename" using execvp
char *args[] = {"grep", "hello", "/etc/hosts", NULL};
execvp("grep", args);
```

### Key Summary

```
fork() + exec() + wait() = the basic pattern for running a new program

Parent:  fork() --> wait() --> continue execution
           |
Child:   exec("ls") --> (ls runs and exits)
```

Why are fork and exec separated?
- Between fork and exec, you can manipulate the child process's environment (fd redirection, setting environment variables, etc.).
- This is one of the key design principles of the Unix philosophy.

---

## Lab 3: Inter-Process Communication with pipe() (~13 min)

### Background

`pipe()` creates a buffer inside the kernel and returns 2 file descriptors for reading and writing.

```
Result of pipe(fd):

  Writer process --write()--> fd[1] ===[kernel buffer]=== fd[0] --read()--> Reader process
                              (write end)                  (read end)
```

Important rules:
- Unused fds **must be closed with close()**.
- The read side's `read()` returns EOF (0) only when all write ends (`fd[1]`) are closed.

### Lab Steps

**Step 1**: Compile and run the example code.

```bash
gcc -Wall -o lab3_pipe_example lab3_pipe_example.c
./lab3_pipe_example
```

**Step 2**: Observe the output and answer the following questions.

- Q1: In Demo 1, why does the parent `close(fd[0])` and the child `close(fd[1])`?
- Q2: How many pipes are used for bidirectional communication in Demo 2? Why can't you use just 1?
- Q3: In Demo 3, what does `dup2(fd[1], STDOUT_FILENO)` do?

**Step 3**: Try modifying it yourself

Modify Demo 1 so that the parent sends multiple lines of messages, and the child reads and prints them line by line.

```c
// Hint: Create a FILE* from fd using fdopen() in the child, then use fgets()
FILE *fp = fdopen(fd[0], "r");
char line[256];
while (fgets(line, sizeof(line), fp) != NULL) {
    printf("[Child] %s", line);
}
fclose(fp);
```

### Key Summary

```c
int fd[2];
pipe(fd);           // fd[0]=read, fd[1]=write
pid_t pid = fork();
if (pid == 0) {
    close(fd[1]);   // Child: close write end
    read(fd[0], buf, sizeof(buf));
    close(fd[0]);
} else {
    close(fd[0]);   // Parent: close read end
    write(fd[1], msg, len);
    close(fd[1]);   // Must close write end so child receives EOF
    wait(NULL);
}
```

---

## Lab 4: dup() and I/O Redirection (~13 min)

### Background

Every process has a file descriptor table:

```
fd number -> file (kernel internal structure)
  0       -> stdin  (keyboard)
  1       -> stdout (screen)
  2       -> stderr (screen)
  3       -> (allocated from here when opened with open)
```

`dup2(oldfd, newfd)` = closes newfd, then makes newfd point to the same file as oldfd.

```
Implementing the shell's "echo hello > output.txt":

1. fd = open("output.txt", O_WRONLY|O_CREAT|O_TRUNC)   -> fd=3
2. dup2(3, 1)     -> stdout(1) now points to output.txt
3. close(3)       -> clean up the original fd
4. exec("echo")   -> echo's stdout output goes to the file!
```

### Lab Steps

**Step 1**: Compile and run the example code.

```bash
gcc -Wall -o lab4_redirect lab4_redirect.c
./lab4_redirect
```

**Step 2**: Observe the output and answer the following questions.

- Q1: In Demo 1, why do we call `close(fd)` after `dup2(fd, STDOUT_FILENO)`?
- Q2: In Demo 3, what is the difference between `dup(fd)` and `dup2(oldfd, newfd)`?
- Q3: How does the pattern in Demo 4 correspond to running `sort < input > output` in a shell?

**Step 3**: Try modifying it yourself

Modify Demo 4 to add functionality that redirects stderr to a separate error file (equivalent to `2> error.txt`).

```c
// Hint: stderr = fd 2
int err_fd = open("error.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(err_fd, STDERR_FILENO);  // stderr -> error.txt
close(err_fd);
```

### Key Summary

```
Shell redirection implementation pattern:

  fork()
    |
    +-- Child:
    |    1. Open file with open()
    |    2. Replace stdin/stdout with dup2()
    |    3. Clean up original fd with close()
    |    4. Run command with exec()
    |
    +-- Parent:
         Wait for child termination with wait()
```

---

## Overall Summary

The relationship between the 4 system calls learned in this lab:

```
+-----------------------------------------------------+
|              How the Shell Works                     |
|                                                      |
|   $ sort < input.txt | grep apple > output.txt       |
|                                                      |
|   1. pipe() -> create pipe                           |
|   2. fork() -> child1 (sort)                         |
|      - dup2(input.txt -> stdin)    [redirection]     |
|      - dup2(pipe_write -> stdout)  [pipe]            |
|      - exec("sort")                                  |
|   3. fork() -> child2 (grep)                         |
|      - dup2(pipe_read -> stdin)    [pipe]            |
|      - dup2(output.txt -> stdout)  [redirection]     |
|      - exec("grep", "apple")                         |
|   4. wait() x 2                                      |
+-----------------------------------------------------+
```

| System Call | Role |
|-------------|------|
| `fork()` | Duplicate process (create new child) |
| `exec()` | Replace process image (run new program) |
| `wait()` | Wait for child termination |
| `pipe()` | Create inter-process communication channel |
| `dup2()` | Replace file descriptor (I/O redirection) |

---

## Next Steps

By combining the system calls learned in this lab, you will implement the following in this week's homework:

1. **pingpong**: A bidirectional communication program using pipes
2. **minishell**: A simple shell combining fork + exec + wait + pipe + dup2

See `../3_assignment/README.md` for the homework specification.
