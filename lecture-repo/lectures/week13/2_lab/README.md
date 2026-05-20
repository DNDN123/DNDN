# Week 13 Lab: Storage Device Management / File System

> **Textbook**: xv6 Chapter 8 - File System
> **Estimated time**: About 50 minutes
> **Type**: Lab only

---

## Learning Objectives

1. Understand the **layered design** of the xv6 file system.
2. Analyze how the **buffer cache** caches and synchronizes disk blocks.
3. Understand how the **logging system** guarantees crash recovery.
4. Trace the file creation/read/write process through **inodes** at the code level.
5. Calculate xv6's file size limits and expansion possibilities.

---

## Background

### File System Layered Structure

The xv6 file system is organized into **6 layers** as shown below.
Each layer uses only the services of the layer directly below it, achieving separation of concerns.

```
  +---------------------------------------------------------+
  |                   User programs                          |
  |              (open, read, write, close)                  |
  +---------------------------------------------------------+
                           |
                    System call boundary
                           |
  +---------------------------------------------------------+
  |  [6] File descriptor layer   (kernel/file.c, sysfile.c) |
  |      - File descriptor table management                  |
  |      - read/write offset tracking                        |
  |      - Unified interface for pipes, devices, inode files |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [5] Pathname layer          (kernel/fs.c: namei, namex) |
  |      - Split pathname by "/" and traverse                |
  |      - Look up each component in the directory           |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [4] Directory layer         (kernel/fs.c: dirlookup,   |
  |      - Directory = special inode (dirent array) dirlink) |
  |      - Name -> inode number mapping                      |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [3] Inode layer             (kernel/fs.c: ialloc, readi,|
  |      - File metadata (type, size, nlink)        writei)  |
  |      - Data block mapping (direct + indirect)            |
  |      - Block allocation/deallocation (balloc, bfree)     |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [2] Logging layer           (kernel/log.c)              |
  |      - Transactions: begin_op() / end_op()               |
  |      - Crash recovery via write-ahead logging             |
  |      - log_write() replaces bwrite()                     |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [1] Buffer cache layer      (kernel/bio.c)              |
  |      - In-memory cache of disk blocks                    |
  |      - bread() / bwrite() / brelse()                     |
  |      - LRU replacement policy, sleep-lock synchronization|
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [0] Disk driver             (kernel/virtio_disk.c)      |
  |      - Performs actual disk I/O via virtio protocol       |
  +---------------------------------------------------------+
```

### xv6 Disk Layout

The xv6 disk is organized as an array of fixed-size blocks (1024 bytes).
The `mkfs` tool generates the disk image with the following layout.

```
Block number:  0      1      2                  33     45    46       2000
         +------+------+-------- ... -------+------+-----+-------- ... --+
         | boot | super|      log           |inode | bit |   data       |
         |block |block |  (31 blocks)       |blocks| map |  blocks      |
         |      |      |  header + 30 data  |(12)  | (1) |  (1954)      |
         +------+------+-------- ... -------+------+-----+-------- ... --+
         |             |                    |      |     |              |
         0             2                    33     45    46            1999
```

**Key constants** (refer to `kernel/param.h`, `kernel/fs.h`, `mkfs/mkfs.c`):

| Constant | Value | Meaning |
|------|-----|------|
| `FSSIZE` | 2000 | Total file system size (in blocks) |
| `BSIZE` | 1024 | Block size (bytes) |
| `LOGBLOCKS` | 30 | Number of log data blocks |
| `nlog` | 31 | Log area = 1 header + 30 data |
| `NINODES` | 200 | Maximum number of inodes |
| `ninodeblocks` | 13 | Number of inode blocks (200 / IPB + 1) |
| `nbitmap` | 1 | Number of bitmap blocks (2000 / 8192 + 1) |
| `NBUF` | 30 | Buffer cache size |
| `NDIRECT` | 12 | Number of direct block pointers |
| `NINDIRECT` | 256 | Number of indirect block pointers (1024/4) |
| `MAXFILE` | 268 | Maximum file size (in blocks: 12+256) |

Role of each region:

| Region | Block Range | Role |
|------|----------|------|
| Boot block | 0 | Boot loader (not used in xv6) |
| Super block | 1 | File system metadata (size, starting position of each region) |
| Log | 2 ~ 32 | Write-ahead log (for crash recovery) |
| Inode blocks | 33 ~ 44 | Array of on-disk inode structures |
| Bitmap | 45 | Bitmap indicating data block usage |
| Data blocks | 46 ~ 1999 | Actual file data |

> **Note**: For the exact block ranges, check the values output when running `mkfs`.
> You can visually verify them by running `examples/lab4_disk_layout.py`.

### Key Data Structures

**superblock** (`kernel/fs.h`):
```c
struct superblock {
  uint magic;        // Must be FSMAGIC (0x10203040)
  uint size;         // Total number of blocks
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes
  uint nlog;         // Number of log blocks
  uint logstart;     // First block of the log area
  uint inodestart;   // First block of the inode area
  uint bmapstart;    // First block of the bitmap area
};
```

**On-disk inode** (`kernel/fs.h`):
```c
struct dinode {
  short type;              // File type (0=free, T_DIR, T_FILE, T_DEVICE)
  short major;             // Device number (T_DEVICE only)
  short minor;
  short nlink;             // Number of hard links
  uint size;               // File size (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses (12 direct + 1 indirect)
};
```

**Buffer** (`kernel/buf.h`):
```c
struct buf {
  int valid;           // Has data been read from disk?
  int disk;            // Is the disk driver currently owning this?
  uint dev;            // Device number
  uint blockno;        // Block number
  struct sleeplock lock;
  uint refcnt;         // Reference count
  struct buf *prev;    // LRU list (previous)
  struct buf *next;    // LRU list (next)
  uchar data[BSIZE];   // Block data (1024 bytes)
};
```

---

## Exercise 1: Buffer Cache Analysis (about 12 min)

> **File**: `kernel/bio.c`, `kernel/buf.h`

The buffer cache is the **lowest software layer** of the file system.
It serves two key roles:

1. **Synchronization**: Serializes concurrent access to the same block
2. **Caching**: Keeps frequently used blocks in memory to reduce disk I/O

### 1-1. `bget()` Function Analysis

`bget()` is the core function of the buffer cache. It finds the requested block in the cache or allocates a new one.

```c
// kernel/bio.c
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // [Step 1] Check if already in cache
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);   // Acquire sleep-lock
      return b;
    }
  }

  // [Step 2] Not in cache, recycle an LRU buffer
  // Search from the end of the list (head.prev) = starting from the oldest
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;        // Not yet read from disk
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}
```

**Question 1-1**: What is the difference between a cache hit (Step 1) and a cache miss (Step 2) in `bget()`?

> **Hint**: Pay attention to the value of the `valid` field. On a cache hit, `valid` is not touched,
> while on a cache miss, `valid` is set to 0.

### 1-2. `bread()` and `bwrite()` Functions

```c
// kernel/bio.c - block read
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
  b = bget(dev, blockno);
  if(!b->valid) {              // If it was a cache miss
    virtio_disk_rw(b, 0);     // Read from disk (0 = read)
    b->valid = 1;
  }
  return b;                    // Returned in locked state
}

// kernel/bio.c - block write
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);       // Write to disk (1 = write)
}
```

### 1-3. `brelse()` - Buffer Release and LRU Update

```c
// kernel/bio.c
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);       // Release sleep-lock

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // If no one is using it -> move to the front of the list (MRU)
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
}
```

**LRU List Structure**:

```
  Most recently used                              Least recently used
       (MRU)                                           (LRU)
         |                                              |
         v                                              v
  head <-> buf_A <-> buf_B <-> buf_C <-> ... <-> buf_Z <-> head
       next->                                        <-prev
```

- On `brelse()`, when `refcnt` becomes 0 -> move to the **front** of the list (head.next) (MRU)
- In `bget()`, when a free buffer is needed -> search from the **back** of the list (head.prev) (LRU)

### 1-4. Observation Questions

**Question 1-2**: Why does the buffer cache use a **sleep-lock** instead of a regular spin-lock?

> **Answer**: Disk I/O is a slow operation that takes several milliseconds.
> Using a spin-lock would waste CPU time waiting for I/O.
> A sleep-lock puts the waiting process to sleep, allowing the CPU to be
> used by other processes. Additionally, a sleep-lock allows a process to
> sleep while holding the lock (which is prohibited with spin-locks).

**Question 1-3**: What does `bcache.lock` (spin-lock) protect versus `b->lock` (sleep-lock)?

> **Answer**:
> - `bcache.lock`: Protects the buffer cache's **linked list structure** and `refcnt` (short critical section)
> - `b->lock`: Protects an individual buffer's **data (`data[]`)** (held during disk I/O)

**Question 1-4**: `NBUF` is 30. What happens if all 30 buffers are in use?

> **Answer**: `bget()` calls `panic("bget: no buffers")` and the kernel halts.
> In a real operating system, the implementation would wait until a buffer in use is released.

---

## Exercise 2: Logging System Analysis (about 12 min)

> **File**: `kernel/log.c`

The xv6 logging system guarantees the **atomicity** of file system operations.
For example, file creation requires multiple block writes such as inode allocation
and directory entry addition. Even if power is lost in the middle,
the file system must maintain a **consistent state**.

### 2-1. Write-Ahead Logging Overview

```
  +-----------+     +-----------+     +-----------+     +-----------+
  | begin_op  | --> | log_write | --> |  end_op   | --> |  commit   |
  | (start    |     | (schedule |     | (if last  |     | (actual   |
  |  txn)     |     |  log write)|    |  op, then |     |  commit)  |
  +-----------+     +-----------+     |  commit)  |     +-----------+
                                      +-----------+
```

**4 stages of commit()**:

```c
// kernel/log.c
static void commit()
{
  if (log.lh.n > 0) {
    write_log();      // [1] Write modified blocks from cache -> log area
    write_head();     // [2] Write log header to disk <- the real commit point!
    install_trans(0); // [3] Copy from log area -> home location
    log.lh.n = 0;
    write_head();     // [4] Set n=0 in log header to erase the log
  }
}
```

### 2-2. `begin_op()` and `end_op()` Analysis

```c
// kernel/log.c
void begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);           // Wait if a commit is in progress
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGBLOCKS){
      sleep(&log, &log.lock);           // Wait if log space is insufficient
    } else {
      log.outstanding += 1;             // Increment outstanding operation count
      release(&log.lock);
      break;
    }
  }
}

void end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;                 // Decrement outstanding operation count
  if(log.outstanding == 0){
    do_commit = 1;                      // If last operation, commit
    log.committing = 1;
  } else {
    wakeup(&log);                       // Wake up waiting begin_op calls
  }
  release(&log.lock);

  if(do_commit){
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);                       // Notify commit completion
    release(&log.lock);
  }
}
```

### 2-3. `log_write()` - Schedule Log Write Instead of Actual Write

```c
// kernel/log.c
void log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGBLOCKS)
    panic("too big a transaction");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)  // log absorption!
      break;                             // Same block: don't add duplicate
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {    // If new block
    bpin(b);               // Pin in cache so it won't be evicted
    log.lh.n++;
  }
  release(&log.lock);
}
```

**Question 2-1**: What is "log absorption" and why is it important?

> **Answer**: It is an optimization where even if the same block is modified multiple times,
> it is recorded only once in the log. For example, even if the same bitmap block is
> modified multiple times, it only takes up one log slot.
> This allows more operations to be processed within the limited log size (`LOGBLOCKS = 30`).

### 2-4. Crash Recovery Scenario Analysis

The outcome depends on where during the 4 stages of `commit()` power is lost:

```
  commit() {
    write_log();       // [1] Write data to log
    ────────── Power loss point A ──────────
    write_head();      // [2] Write n to header <- commit point!
    ────────── Power loss point B ──────────
    install_trans(0);  // [3] Copy data to home location
    ────────── Power loss point C ──────────
    log.lh.n = 0;
    write_head();      // [4] Erase log
  }
```

| Power Loss Point | Log Header State | Action on Reboot | Result |
|:---:|:---:|:---|:---|
| A | `n = 0` | Nothing to recover | Transaction **discarded** (data loss) |
| B | `n > 0` | Execute `install_trans(1)` | Transaction **replayed** (full recovery) |
| C | `n > 0` | Execute `install_trans(1)` | Transaction **replayed** (idempotent, safe) |

**`recover_from_log()` code**:

```c
// kernel/log.c
static void recover_from_log(void)
{
  read_head();           // Read log header from disk
  install_trans(1);      // If n > 0, copy log -> home location (replay)
  log.lh.n = 0;
  write_head();          // Erase log
}
```

**Question 2-2**: Why is `write_head()` (step 2) the "real commit point"?

> **Answer**: `write_head()` writes the log header's `n` value to disk.
> Only after this write completes is the fact that "a committed transaction exists"
> permanently recorded on disk. On reboot, `read_head()` reads this value, and
> if `n > 0`, recovery is performed. The atomicity of the single block write
> (`write_head`) is guaranteed by the disk hardware.

**Question 2-3**: Why is `install_trans()` idempotent?

> **Answer**: `install_trans()` simply **overwrites** the home location with the contents
> of the log blocks. Even if the correct data is already there, writing the same data
> again produces the same result. Therefore, it is safe to execute multiple times.

---

## Exercise 3: Tracing the File Creation Process (about 15 min)

> **Files**: `kernel/sysfile.c`, `kernel/fs.c`, `kernel/file.c`, `kernel/log.c`, `kernel/bio.c`

Let's trace step by step the code path executed when a user program calls
`open("newfile", O_CREATE|O_RDWR)`.

### 3-1. Complete Call Chain

```
  User: open("newfile", O_CREATE|O_RDWR)
    |
    v
  [sysfile.c] sys_open()
    |-- begin_op()                    // Start transaction
    |-- create("newfile", T_FILE, 0, 0)
    |     |-- nameiparent("newfile")  // Find parent directory (/) inode
    |     |-- ilock(dp)              // Lock parent directory
    |     |-- dirlookup(dp, "newfile") // Check if already exists
    |     |-- ialloc(dev, T_FILE)    // Allocate new inode *
    |     |     |-- bread(dev, IBLOCK(inum))  // Read inode block
    |     |     |-- log_write(bp)             // Log modified block
    |     |     `-- brelse(bp)
    |     |-- ilock(ip)              // Lock new inode
    |     |-- iupdate(ip)           // Write nlink=1 to disk
    |     |     |-- bread(...)
    |     |     |-- log_write(...)
    |     |     `-- brelse(...)
    |     |-- dirlink(dp, "newfile", ip->inum)  // Add entry to directory *
    |     |     `-- writei(dp, ...)  // Write to directory inode
    |     |           |-- bmap(dp, ...) // Block mapping (balloc if needed)
    |     |           |-- bread(...)
    |     |           |-- log_write(...)
    |     |           `-- brelse(...)
    |     `-- iunlockput(dp)         // Release parent directory
    |
    |-- filealloc()                  // Allocate slot in system-wide file table
    |-- fdalloc(f)                   // Allocate slot in process file descriptor table
    |-- f->type = FD_INODE
    |-- f->ip = ip
    |-- f->readable = 1, f->writable = 1
    |-- iunlock(ip)                  // Unlock inode (reference retained)
    |-- end_op()                     // End transaction -> commit
    `-- return fd                    // Return file descriptor
```

### 3-2. Key Function Code

**`sys_open()`** O_CREATE path (`kernel/sysfile.c`):

```c
uint64 sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;

  argint(1, &omode);
  argstr(0, path, MAXPATH);

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);  // Create file (returned in locked state)
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    // ... open existing file ...
  }

  f = filealloc();           // Allocate struct file
  fd = fdalloc(f);           // Allocate fd number

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);
  end_op();

  return fd;
}
```

**`create()`** (`kernel/sysfile.c`):

```c
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  dp = nameiparent(path, name);  // Parent directory inode
  ilock(dp);

  // Check if already exists
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;     // Return existing file
    iunlockput(ip);
    return 0;
  }

  ip = ialloc(dp->dev, type);   // Allocate new inode
  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);                   // Write to disk

  dirlink(dp, name, ip->inum);  // Add entry to directory
  iunlockput(dp);

  return ip;   // Returned in locked state
}
```

### 3-3. write() System Call Trace

The flow when writing data to a file:

```
  User: write(fd, "hello", 5)
    |
    v
  [sysfile.c] sys_write()
    |-- argfd(0, 0, &f)              // Convert fd -> struct file*
    `-- filewrite(f, addr, 5)
          |
          v
        [file.c] filewrite()
          |-- begin_op()              // Start transaction
          |-- ilock(f->ip)            // Lock inode
          |-- writei(f->ip, 1, addr, f->off, n)
          |     |
          |     v
          |   [fs.c] writei()
          |     |-- bmap(ip, off/BSIZE)  // Logical block -> physical block
          |     |     `-- balloc()       // Allocate new block if needed
          |     |-- bread(ip->dev, addr) // Read block
          |     |-- either_copyin(...)   // Copy user data -> buffer
          |     |-- log_write(bp)        // Schedule log write
          |     |-- brelse(bp)           // Release buffer
          |     `-- iupdate(ip)          // Update inode's size
          |
          |-- f->off += r               // Update file offset
          |-- iunlock(f->ip)             // Unlock inode
          `-- end_op()                   // End transaction -> commit
```

### 3-4. read() System Call Trace

```
  User: read(fd, buf, 5)
    |
    v
  [sysfile.c] sys_read()
    `-- fileread(f, addr, 5)
          |
          v
        [file.c] fileread()
          |-- ilock(f->ip)
          |-- readi(f->ip, 1, addr, f->off, n)
          |     |
          |     v
          |   [fs.c] readi()
          |     |-- bmap(ip, off/BSIZE)     // Logical block -> physical block
          |     |-- bread(ip->dev, addr)    // Read block (check cache)
          |     |-- either_copyout(...)     // Copy buffer -> user space
          |     `-- brelse(bp)
          |
          |-- f->off += r
          `-- iunlock(f->ip)
```

**Question 3-1**: `read()` does not call `begin_op()/end_op()`. Why?

> **Answer**: `read()` does **not modify** the disk. Therefore, crash recovery is not needed
> and a log transaction is unnecessary. In contrast, `write()` modifies data blocks,
> inodes, bitmaps, and other blocks, so a transaction is required.

**Question 3-2**: Why does `filewrite()` only write `((MAXOPBLOCKS-1-1-2)/2)*BSIZE` bytes at a time?

> **Answer**: The number of blocks that can be written in a single transaction is limited to
> `MAXOPBLOCKS(=10)`. A write operation requires not only data blocks but also
> an inode block (1), an indirect block (1), and bitmap blocks (2, since they may
> not be aligned). Therefore, the number of blocks available for pure data is
> `(10-1-1-2)/2 = 3`, and only `3 * 1024 = 3072 bytes` can be written at once.
> Large files are written across multiple transactions.

### 3-5. Lab Exercise: lab3_fs_trace Program

`examples/lab3_fs_trace.c` is an xv6 user program that prints the file creation/write/read
process step by step. Run it in xv6 to observe each step.

```
$ lab3_fs_trace
```

Read the source code of this program and map each system call to the code paths
analyzed above.

---

## Exercise 4: Large File Support Analysis (about 11 min)

> **Files**: `kernel/fs.h`, `kernel/fs.c` `bmap()` function

### 4-1. Current xv6 File Size Limit

The xv6 inode has the following block address structure:

```
  struct dinode {
    ...
    uint addrs[NDIRECT+1];   // addrs[0..11]: direct blocks
  };                          // addrs[12]:    indirect block pointer

  addrs[0]  -> data block 0     -+
  addrs[1]  -> data block 1      |
  addrs[2]  -> data block 2      | 12 direct blocks
  ...                             | = 12 * 1024 = 12,288 bytes
  addrs[11] -> data block 11    -+

  addrs[12] -> [indirect block] -+
                  |               |
                  v               | 256 indirect blocks
              +--------+          | = 256 * 1024 = 262,144 bytes
              | addr 0 | -> data block 12
              | addr 1 | -> data block 13
              | ...    |
              |addr 255| -> data block 267
              +--------+          |
                                 -+
```

**Maximum file size calculation**:

```
  NDIRECT = 12
  NINDIRECT = BSIZE / sizeof(uint) = 1024 / 4 = 256
  MAXFILE = NDIRECT + NINDIRECT = 12 + 256 = 268 blocks
  Maximum file size = 268 * 1024 bytes = 274,432 bytes ~ 268 KB
```

### 4-2. `bmap()` Function Analysis

`bmap()` translates a file's logical block number to a physical block number on disk.

```c
// kernel/fs.c
static uint bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  // Direct blocks (bn = 0..11)
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);    // Allocate if not yet allocated
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  // Indirect blocks (bn = 0..255, originally 12..267)
  if(bn < NINDIRECT){
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);    // Allocate the indirect block itself
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);   // Read indirect block
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);    // Allocate data block
      a[bn] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");   // Error if exceeding 268 blocks
}
```

### 4-3. Extension Calculation When Adding a Double Indirect Block

If a **double indirect** pointer is added to the `addrs[]` array:

```
  addrs[0..11]  : 12 direct blocks
  addrs[12]     : 1 single indirect block -> 256 data blocks
  addrs[13]     : 1 double indirect block (newly added!)
                     |
                     v
                 +--------+
                 | ptr 0  | -> [indirect block 0] -> 256 data blocks
                 | ptr 1  | -> [indirect block 1] -> 256 data blocks
                 | ...    |
                 |ptr 255 | -> [indirect block 255] -> 256 data blocks
                 +--------+
                  256 pointers * 256 data blocks = 65,536 blocks
```

**Extended maximum file size calculation**:

```
  Direct blocks:        12 blocks
  Single indirect:     256 blocks
  Double indirect: 256 * 256 = 65,536 blocks
  ──────────────────────────
  Total:             65,804 blocks

  Maximum file size = 65,804 * 1024 bytes
                    = 67,383,296 bytes
                    ~ 64.25 MB
```

**Question 4-1**: In the current `dinode` struct, the size of `addrs[]` is `NDIRECT+1 = 13`.
How should `addrs[]` be changed to add double indirect support?

> **Answer**: Change to `addrs[NDIRECT+2]` (size 14). `addrs[12]` is used for single indirect
> and `addrs[13]` for double indirect. The `MAXFILE` macro must also be changed to
> `NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT`.

**Question 4-2**: What would the maximum file size be if a triple indirect block were added?

> **Answer**:
> ```
>   Direct:                12 blocks
>   Single indirect:      256 blocks
>   Double indirect: 256 * 256 = 65,536 blocks
>   Triple indirect: 256^3    = 16,777,216 blocks
>   Total:                  16,843,020 blocks
>   = 16,843,020 * 1024 bytes ~ 16 GB
> ```

### 4-4. `bmap()` Extension Pseudocode

Pseudocode for extending `bmap()` to support double indirect:

```c
static uint bmap(struct inode *ip, uint bn)
{
  // ... existing direct, single indirect code ...

  bn -= NINDIRECT;

  // Double indirect (bn = 0 .. NINDIRECT*NINDIRECT - 1)
  if(bn < NINDIRECT * NINDIRECT){
    // Stage 1: Load the double indirect block itself
    if((addr = ip->addrs[NDIRECT+1]) == 0){
      addr = balloc(ip->dev);
      ip->addrs[NDIRECT+1] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    // Stage 2: Load the intermediate indirect block (the bn/NINDIRECT-th one)
    uint idx1 = bn / NINDIRECT;
    if((addr = a[idx1]) == 0){
      addr = balloc(ip->dev);
      a[idx1] = addr;
      log_write(bp);
    }
    brelse(bp);

    // Stage 3: Final data block
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    uint idx2 = bn % NINDIRECT;
    if((addr = a[idx2]) == 0){
      addr = balloc(ip->dev);
      a[idx2] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```

---

## Summary and Key Takeaways

### File System Layer Summary

| Layer | Source File | Key Functions | Role |
|------|----------|----------|------|
| File descriptor | `file.c`, `sysfile.c` | `fileread`, `filewrite`, `sys_open` | User interface |
| Pathname | `fs.c` | `namei`, `namex` | Pathname resolution |
| Directory | `fs.c` | `dirlookup`, `dirlink` | Name-to-inode mapping |
| Inode | `fs.c` | `ialloc`, `readi`, `writei`, `bmap` | File metadata and data blocks |
| Log | `log.c` | `begin_op`, `end_op`, `log_write` | Crash recovery |
| Buffer cache | `bio.c` | `bread`, `bwrite`, `brelse` | Block caching, synchronization |

### Key Concepts

1. **Buffer cache**: LRU replacement policy, concurrency control with sleep-locks
   - `bcache.lock` (spin-lock): Protects list structure
   - `b->lock` (sleep-lock): Protects buffer data (can sleep during I/O)

2. **Logging**: Write-ahead log guarantees atomicity
   - `write_head()` is the real commit point
   - Automatic recovery via `recover_from_log()` on reboot
   - Log absorption saves space

3. **Inode**: 12 direct + 1 indirect = max 268 blocks (268 KB)
   - `bmap()`: Logical block -> physical block translation (allocates if needed)
   - `ialloc()`: Searches for and allocates a free inode

4. **File creation flow**: `sys_open` -> `create` -> `ialloc` + `dirlink`
   - All modifications are performed within a `begin_op()`/`end_op()` transaction
   - `log_write()` replaces `bwrite()` for logging

5. **File size extension**: Adding double indirect supports up to about 64 MB
