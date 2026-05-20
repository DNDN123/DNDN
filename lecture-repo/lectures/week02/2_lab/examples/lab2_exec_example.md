# Lab 2: exec()

## Description
Demonstrates the `exec()` family of functions (`execl`, `execlp`, `execv`, `execvp`) which replace the current process image with a new program. Each demo runs inside a fork-exec-wait pattern so the parent can continue after the child calls exec.

## Build & Run
```bash
gcc -Wall -o lab2_exec_example lab2_exec_example.c
./lab2_exec_example
```

## What to Observe
- After a successful `exec()`, the code following the call is never executed (the process image is replaced)
- Each exec variant differs in how it accepts arguments (list vs. array) and whether it searches `PATH`
- In Demo 6, exec fails because the program does not exist, so `perror` prints an error message and the child exits with code 127
- The PID remains the same before and after exec -- only the program changes

## Try It Yourself
- Add a new demo to `lab2_exec_example.c`: use `execvp` to run `grep` and search for a pattern in a specific file. Hint:
  ```c
  char *args[] = {"grep", "hello", "/etc/hosts", NULL};
  execvp("grep", args);
  ```

## Discussion Questions
- Q1: Does the `printf()` after the `exec()` call get executed? Why or why not?
- Q2: In Demo 6, why does `perror` print when exec fails?
- Q3: What happens if you call `exec()` without `fork()`?
