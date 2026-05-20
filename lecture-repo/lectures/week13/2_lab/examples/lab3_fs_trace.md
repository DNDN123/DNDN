# Lab 3: File Creation Tracing (fs_trace.c)

## Description

`lab3_fs_trace.c` is an xv6 user program that traces the complete lifecycle of
file system operations: file creation, writing, reading, verification, directory
creation, and file deletion. At each step, it prints the internal kernel call
chain so you can see exactly which functions are invoked (e.g., `sys_open` ->
`create` -> `ialloc` -> `dirlink`).

The program walks through 10 stages:

1. File creation (`open` with `O_CREATE`)
2. First write (new block allocation via `balloc`)
3. Second write (reusing an already-allocated block)
4. File close (`fileclose` -> `iput`)
5. Re-open without `O_CREATE` (`namei` -> `dirlookup`)
6. File read (`readi` -> `bread`, no transaction needed)
7. Data verification (compare written vs. read content)
8. Directory creation (`mkdir` -> `ialloc` with `T_DIR`)
9. Nested file creation (path resolution via `namex`)
10. File deletion (`unlink` -> `nlink--` -> block deallocation)

## How to Use

This is an **xv6 user program** and must be compiled inside the xv6 build system.

```bash
# 1. Copy the source file into the xv6 user directory
cp lab3_fs_trace.c /path/to/xv6-riscv/user/fs_trace.c

# 2. Edit xv6-riscv/Makefile: add to the UPROGS list
#    $U/_fs_trace\

# 3. Build and run xv6
cd /path/to/xv6-riscv
make qemu

# 4. Inside the xv6 shell, run:
$ fs_trace
```

## What to Observe

- **Stage 1 (create)**: Note how `ialloc` scans inode blocks to find a free
  inode, and `dirlink` adds the new name to the parent directory.
- **Stage 2 vs. Stage 3 (write)**: The first write triggers `balloc` to allocate
  a new data block. The second write reuses the same block since the data fits
  within it -- `bmap` returns the existing block address.
- **Stage 4 (close)**: Observe that `fileclose` decrements the reference count
  and calls `iput` inside a transaction.
- **Stage 5 (re-open)**: Without `O_CREATE`, `sys_open` takes a different code
  path: `namei` -> `dirlookup` to find the existing inode.
- **Stage 6 (read)**: Reading does **not** use `begin_op`/`end_op` because no
  disk modifications occur.
- **Stage 7 (verify)**: Confirms that the data round-tripped correctly through
  write -> log -> disk -> buffer cache -> read.
- **Stage 8 (mkdir)**: A directory inode gets `.` and `..` entries automatically.
- **Stage 10 (unlink)**: `nlink` is decremented; when it reaches 0 and no open
  references remain, `itrunc` frees all data blocks.

## Discussion Questions

1. Why does file creation require a **transaction** (`begin_op`/`end_op`) while
   reading does not?
2. In Stage 2, `bmap` calls `balloc` to allocate a block. What would happen if
   the system crashed between `balloc` and `log_write`? Would the block be leaked?
3. The program creates a directory in Stage 8. Why does `mkdir` increment the
   **parent directory's** `nlink` count?
4. In Stage 10, `unlink` sets the directory entry's inode number to 0. What
   happens if another process has the file open when `unlink` is called?
5. Compare the call paths for `open` with `O_CREATE` (Stage 1) versus without
   (Stage 5). Which layers of the file system are bypassed when the file already
   exists?
