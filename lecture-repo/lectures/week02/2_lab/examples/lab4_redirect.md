# Lab 4: dup2() and I/O Redirection

## Description
Demonstrates how `dup2()` redirects standard I/O by making one file descriptor point to the same file as another. Covers output redirection (`> file`), input redirection (`< file`), the difference between `dup()` and `dup2()`, and the full shell redirection pattern (`sort < input > output`).

## Build & Run
```bash
gcc -Wall -o lab4_redirect lab4_redirect.c
./lab4_redirect
```

## What to Observe
- In Demo 1, after `dup2(fd, STDOUT_FILENO)`, echo's output goes to the file instead of the terminal
- After calling `dup2()`, the original fd is closed with `close(fd)` because it is no longer needed -- stdout now points to the file
- In Demo 3, `dup(fd)` returns a new fd (the lowest available number) that points to the same file, and both fds can write to it
- In Demo 4, the child opens an input file on stdin and an output file on stdout, then execs `sort` -- this is exactly how a shell implements `sort < input > output`
- Temporary files are cleaned up with `unlink()` after each demo

## Try It Yourself
- Modify Demo 4 to add stderr redirection to a separate error file (equivalent to `2> error.txt`). Hint:
  ```c
  int err_fd = open("error.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  dup2(err_fd, STDERR_FILENO);
  close(err_fd);
  ```

## Discussion Questions
- Q1: In Demo 1, why do we call `close(fd)` after `dup2(fd, STDOUT_FILENO)`?
- Q2: In Demo 3, what is the difference between `dup(fd)` and `dup2(oldfd, newfd)`?
- Q3: How does the pattern in Demo 4 correspond to running `sort < input > output` in a shell?
