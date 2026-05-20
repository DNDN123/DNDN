# Lab 3: pipe()

## Description
Demonstrates inter-process communication using `pipe()`. Covers one-way parent-to-child messaging, bidirectional communication with two pipes, and connecting two commands together (simulating shell pipe `echo | wc`).

## Build & Run
```bash
gcc -Wall -o lab3_pipe_example lab3_pipe_example.c
./lab3_pipe_example
```

## What to Observe
- In Demo 1, the parent writes a message to `fd[1]` and the child reads it from `fd[0]` -- data flows in one direction only
- Each side closes the pipe end it does not use (the parent closes `fd[0]`, the child closes `fd[1]`)
- In Demo 2, two separate pipes are needed for bidirectional communication because a single pipe is unidirectional
- In Demo 3, `dup2(fd[1], STDOUT_FILENO)` redirects echo's stdout into the pipe, and `dup2(fd[0], STDIN_FILENO)` connects the pipe to wc's stdin
- The parent must close both pipe ends so that the reading child eventually receives EOF

## Try It Yourself
- Modify Demo 1 so that the parent sends multiple lines of messages, and the child reads and prints them line by line. Hint: use `fdopen()` and `fgets()` in the child:
  ```c
  FILE *fp = fdopen(fd[0], "r");
  char line[256];
  while (fgets(line, sizeof(line), fp) != NULL) {
      printf("[Child] %s", line);
  }
  fclose(fp);
  ```

## Discussion Questions
- Q1: In Demo 1, why does the parent `close(fd[0])` and the child `close(fd[1])`?
- Q2: How many pipes are used for bidirectional communication in Demo 2? Why can't you use just 1?
- Q3: In Demo 3, what does `dup2(fd[1], STDOUT_FILENO)` do?
