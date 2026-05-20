# Week 2 Homework: Process 1 - fork, exec, wait, pipe, I/O

> Textbook: xv6 Ch 1 - Operating System Interfaces
> Deadline: Before the start of the next class
> Submission: Source code files (`pingpong.c`, `minishell.c`)

---

## Overview

There are 2 assignments this week. Both are **required**.

| Assignment | Difficulty | Key Concepts | File |
|------------|------------|--------------|------|
| 1. pingpong | Basic | pipe, fork, read/write | `pingpong.c` |
| 2. minishell | Intermediate | fork, exec, wait, pipe, dup2 | `minishell.c` |

---

## Environment

```bash
# Required tools
gcc --version   # gcc required

# Compilation method (common for all assignments)
gcc -Wall -o output source.c

# Run tests
cd tests/
chmod +x *.sh
./test_pingpong.sh ../skeleton/pingpong.c
./test_minishell.sh ../skeleton/minishell.c
```

---

## Assignment 1: pingpong (Required)

### Description

This assignment is based on xv6 textbook Ch 1 Exercise 1.

Write a program where two processes (parent and child) exchange 1 byte through pipes.

```
Parent --[ping: 1 byte]--> Child
Parent <--[pong: 1 byte]-- Child
```

### Behavior

1. The parent sends 1 byte (`'p'`) to the child (ping).
2. The child receives the byte and prints `<pid>: received ping`.
3. The child sends 1 byte back to the parent (pong).
4. The parent receives the byte and prints `<pid>: received pong`.
5. The round-trip time is measured and printed in microseconds.

### Output Format (must follow this format exactly)

```
<childPID>: received ping
<parentPID>: received pong
Round-trip time: X.XXX us
```

Example:

```
12345: received ping
12344: received pong
Round-trip time: 156.000 us
```

### Implementation Guide

The skeleton file `skeleton/pingpong.c` contains TODO comments. Implement each TODO in order.

**Required system calls**: `pipe()`, `fork()`, `read()`, `write()`, `close()`, `wait()`, `getpid()`

**Key points**:

1. **2 pipes** are needed for bidirectional communication.
2. Unused pipe fds in each process must be `close()`d.
3. The child receives the ping, and the parent receives the pong.

**Hints** (refer to these if you get stuck):

<details>
<summary>Hint 1: Pipe configuration</summary>

```
parent_to_child[2]:  Parent writes to [1], child reads from [0]
child_to_parent[2]:  Child writes to [1], parent reads from [0]
```

</details>

<details>
<summary>Hint 2: fds the child process should close</summary>

```c
// Child only reads from parent_to_child and only writes to child_to_parent
close(parent_to_child[1]);  // Close write end
close(child_to_parent[0]);  // Close read end
```

</details>

<details>
<summary>Hint 3: How to use read/write</summary>

```c
char buf = 'p';
write(parent_to_child[1], &buf, 1);  // Write 1 byte
read(parent_to_child[0], &buf, 1);   // Read 1 byte
```

</details>

### Testing

```bash
cd tests/
./test_pingpong.sh ../skeleton/pingpong.c
```

Test items:
- Compilation success
- Normal termination
- "received ping" / "received pong" output verification
- PIDs are different from each other
- Round-trip time output verification
- Stability across repeated runs

---

## Assignment 2: minishell (Required)

### Description

Implement a simple shell. Through this assignment, you will understand how a real shell executes commands.

### Supported Features

| Feature | Example | Description |
|---------|---------|-------------|
| Single command | `ls -l` | fork + execvp + wait |
| Output redirection | `echo hello > file.txt` | stdout to file |
| Input redirection | `sort < data.txt` | stdin from file |
| Pipe | `ls \| wc -l` | cmd1's stdout to cmd2's stdin |
| Exit | `exit` | Exit the shell |

### Implementation Guide

The skeleton file `skeleton/minishell.c` already has the following implemented:
- `main()` function (reading input, pipe splitting, exit handling)
- `parse_command()` function (command parsing, redirection parsing)
- `struct command` structure

**Functions students must implement**:

#### 1. `execute_command(struct command *cmd)`

Executes a single command.

```
[Implementation steps]
1. Create a child with fork()
2. In the child:
   - If cmd->infile exists: open() -> dup2(fd, STDIN_FILENO) -> close(fd)
   - If cmd->outfile exists: open() -> dup2(fd, STDOUT_FILENO) -> close(fd)
   - Execute execvp(cmd->argv[0], cmd->argv)
   - On exec failure, print error and exit(127)
3. In the parent: wait for child termination with waitpid()
```

#### 2. `execute_pipe(struct command *cmd1, struct command *cmd2)`

Executes two commands connected by a pipe.

```
[Implementation steps]
1. Create a pipe with pipe(fd)
2. Create child1 with fork() (runs cmd1):
   - Close fd[0]
   - dup2(fd[1], STDOUT_FILENO) to redirect stdout to pipe write end
   - close(fd[1])
   - If cmd1->infile exists, apply input redirection
   - execvp(cmd1->argv[0], cmd1->argv)
3. Create child2 with fork() (runs cmd2):
   - Close fd[1]
   - dup2(fd[0], STDIN_FILENO) to redirect stdin to pipe read end
   - close(fd[0])
   - If cmd2->outfile exists, apply output redirection
   - execvp(cmd2->argv[0], cmd2->argv)
4. In the parent: close fd[0] and fd[1], waitpid for both children
```

### Implement execute_command first

`execute_pipe` is an extension of `execute_command`. Complete and test `execute_command` first, then implement `execute_pipe`.

**Hints** (refer to these if you get stuck):

<details>
<summary>Hint 1: Basic structure of execute_command</summary>

```c
pid_t pid = fork();
if (pid == 0) {
    // Child: set up redirection then exec
    execvp(cmd->argv[0], cmd->argv);
    perror("minishell");
    exit(127);
}
// Parent: wait
int status;
waitpid(pid, &status, 0);
```

</details>

<details>
<summary>Hint 2: Input redirection</summary>

```c
if (cmd->infile != NULL) {
    int fd = open(cmd->infile, O_RDONLY);
    if (fd < 0) {
        perror(cmd->infile);
        exit(1);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}
```

</details>

<details>
<summary>Hint 3: What the parent must do in execute_pipe</summary>

The parent process **must** close both ends of the pipe. Otherwise, child2's `read()` will never receive EOF and will block forever.

```c
close(fd[0]);
close(fd[1]);
waitpid(pid1, NULL, 0);
waitpid(pid2, &status, 0);
```

</details>

### Testing

```bash
cd tests/
./test_minishell.sh ../skeleton/minishell.c
```

Test items:
- Compilation success
- `echo` command execution
- Command argument handling
- Output redirection (`>`)
- Input redirection (`<`)
- Pipe (`|`)
- Pipe + redirection combination
- `exit` command
- EOF handling
- Non-existent command error handling
- Empty input handling
- Sequential command execution

### Manual Testing

In addition to automated tests, run the shell yourself and try various commands:

```bash
$ ./minishell
minishell> ls
minishell> echo hello world
hello world
minishell> echo test > /tmp/test.txt
minishell> cat /tmp/test.txt
test
minishell> cat < /tmp/test.txt
test
minishell> ls | wc -l
minishell> echo hello | cat > /tmp/pipe_test.txt
minishell> cat /tmp/pipe_test.txt
hello
minishell> exit
```

---

## Submission Method

Submit the following files:
1. `pingpong.c` - Completed pingpong program
2. `minishell.c` - Completed minishell program

### Grading Criteria

| Item | Score |
|------|-------|
| pingpong - Compilation and normal execution | 10% |
| pingpong - Correct output format | 15% |
| pingpong - Round-trip time measurement | 5% |
| minishell - Single command execution | 20% |
| minishell - Output redirection | 15% |
| minishell - Input redirection | 15% |
| minishell - Pipe | 20% |

### Notes

- **All code must compile without warnings using `gcc -Wall`.**
- Must work on both macOS and Linux.
- Include error handling for `fork()` failure, `exec()` failure, `open()` failure, etc.
- Unused pipe fds must be closed (otherwise the program may hang).
- Memory leaks will not be penalized in this assignment.

---

## References

- xv6 textbook Ch 1: Operating System Interfaces (especially sections 1.1, 1.2, 1.3)
- `man fork`, `man exec`, `man wait`, `man pipe`, `man dup2`
- Lab example code: `../2_lab/examples/`
