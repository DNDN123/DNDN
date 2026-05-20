# Lab 1: fork() and wait()

## Description
Demonstrates how `fork()` duplicates a process to create a child and how `wait()` blocks the parent until the child terminates. The example also shows how to retrieve the child's exit code using `WEXITSTATUS()` and how to create multiple children in a loop.

## Build & Run
```bash
gcc -Wall -o lab1_fork_basic lab1_fork_basic.c
./lab1_fork_basic
```

## What to Observe
- The parent's PID and the child's PID are different, but `getppid()` in the child matches the parent's PID
- The child sleeps for 1 second before exiting, and the parent blocks on `wait()` during that time
- The parent retrieves the child's exit code (42) via `WEXITSTATUS(status)`
- In the multi-child section, the termination order may differ from the creation order due to scheduling

## Try It Yourself
- Modify `lab1_fork_basic.c` so that 5 child processes are created. Have each child exit with a different exit code (0 through 4), and have the parent print all the exit codes.

## Discussion Questions
- Q1: Who prints first, the parent or the child? Is it the same every time you run it?
- Q2: What happens if you remove `wait()`? (Comment it out and recompile)
- Q3: When the child exits with `exit(42)`, how can the parent retrieve the value 42?
