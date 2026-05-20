/*
 * fs_trace.c - xv6 File System Operation Tracing Program
 *
 * [Overview]
 * From file creation, writing, closing, reopening, reading, to verification,
 * each step is clearly printed to understand file system behavior.
 * This program is an xv6 user program that runs inside xv6.
 *
 * [xv6 File System Layer Structure]
 *
 *   User Program       : open(), read(), write(), close(), mkdir(), unlink()
 *        |
 *   System Call Layer   : sys_open(), sys_read(), sys_write(), sys_close() ...
 *        |
 *   File Descriptor     : file.c - struct file, fileread(), filewrite()
 *        |               fd -> struct file -> struct inode
 *   Path Resolution     : fs.c - namei(), namex(), dirlookup(), dirlink()
 *        |               Convert path string to inode (e.g., "/testdir/inner.txt")
 *   Inode Layer         : fs.c - ialloc(), iget(), ilock(), iput(), readi(), writei()
 *        |               Manage disk inode and in-memory inode
 *   Log Layer           : log.c - begin_op(), end_op(), log_write(), commit()
 *        |               Ensure crash consistency via WAL (Write-Ahead Logging)
 *   Buffer Cache Layer  : bio.c - bread(), bwrite(), brelse()
 *        |               Cache disk blocks in memory, LRU replacement policy
 *   Disk Driver         : virtio_disk.c - virtio_disk_rw()
 *                       Perform actual disk I/O via VirtIO interface
 *
 * [Role of Each Layer]
 *   - Buffer Cache (bio.c): Caches disk blocks in memory to reduce I/O count
 *     bcache.lock (spinlock) protects the list, buf.lock (sleeplock) protects data
 *   - Log (log.c): Groups all disk modifications into transactions for atomicity
 *     On recovery after crash, replays the log to maintain consistency
 *   - Inode (fs.c): Manages file metadata (size, type, block pointers)
 *     In-memory inode (struct inode) lifetime managed by reference count (ref)
 *   - Directory (fs.c): Manages name -> inode number mapping via directory entries
 *     Each entry is struct dirent (inum + name[14])
 *
 * Build instructions:
 *   1. Copy this file to xv6-riscv/user/fs_trace.c
 *   2. Add $U/_fs_trace to UPROGS in Makefile
 *   3. Build and run with make qemu
 *
 * Run in xv6 shell:
 *   $ fs_trace
 */

/* xv6 header file includes
 * kernel/types.h: Basic type definitions such as uint, uint64
 * kernel/stat.h : struct stat definition (type, ino, nlink, size)
 * user/user.h   : System call wrapper function declarations (open, read, write, close, mkdir, etc.)
 * kernel/fcntl.h: File open flag definitions (O_RDONLY, O_WRONLY, O_RDWR, O_CREATE) */
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Print separator line (for visual separation) */
void
print_separator(void)
{
  printf("--------------------------------------------------\n");
}

/*
 * print_stat - Print file metadata (inode information).
 *
 * The fstat() system call returns the struct stat of an open file.
 * xv6's struct stat (kernel/stat.h):
 *   - type  : File type (T_DIR=1, T_FILE=2, T_DEVICE=3)
 *   - ino   : Inode number (index into the disk inode array)
 *   - nlink : Hard link count (number of directory entries pointing to this inode)
 *             When nlink reaches 0, the inode and data blocks are freed
 *   - size  : File size (bytes)
 */
void
print_stat(char *path)
{
  struct stat st;
  int fd;

  /* Open the file to get fd, then query metadata with fstat
   * xv6 internals: sys_fstat() -> stati() -> copy inode info to stat struct */
  fd = open(path, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] open(\"%s\") failed\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf("  [ERROR] fstat failed\n");
    close(fd);
    return;
  }

  /* Print metadata stored in the inode */
  printf("  File info:\n");
  printf("    - type  : %d ", st.type);
  if(st.type == 1) printf("(T_DIR)\n");       /* Directory */
  else if(st.type == 2) printf("(T_FILE)\n"); /* Regular file */
  else if(st.type == 3) printf("(T_DEVICE)\n"); /* Device file (console, etc.) */
  else printf("(unknown)\n");
  printf("    - ino   : %d\n", st.ino);   /* Inode number: index into disk inode array */
  printf("    - nlink : %d\n", st.nlink); /* Hard link count */
  printf("    - size  : %d bytes\n", st.size); /* File size */

  close(fd);
}

int
main(int argc, char *argv[])
{
  int fd;                              /* File descriptor (index into per-process fd table) */
  int n;                               /* Number of bytes read/written */
  char buf[128];                       /* Read buffer */
  char *testfile = "testfile.txt";     /* Test file name */
  char *testdir  = "testdir";          /* Test directory name */
  char *msg1 = "Hello, xv6 file system!\n";  /* First write data */
  char *msg2 = "Second write to file.\n";    /* Second write data */

  (void)argc;  /* Suppress unused argument warning */
  (void)argv;

  printf("==================================================\n");
  printf("  xv6 File System Operation Tracing Program\n");
  printf("==================================================\n\n");

  /* --------------------------------------------------------
   * Step 1: File Creation
   * When sys_open() has the O_CREATE flag, it enters the create() path
   * create() allocates a new inode and adds an entry to the parent directory
   *
   * ialloc(): Scans the inode block area on disk, finds a free inode
   *           with type==0 and allocates it (sets type to T_FILE)
   * dirlink(): Adds a new dirent to the parent directory's data block
   *            (inum + name[14] = 16-byte entry)
   * -------------------------------------------------------- */
  printf("[Step 1] File creation: open(\"%s\", O_CREATE|O_RDWR)\n", testfile);
  printf("  Call path:\n");
  printf("    sys_open() -> create() -> nameiparent()\n");
  printf("                            -> ialloc()  : Allocate free inode\n");
  printf("                            -> dirlink() : Add entry to directory\n");
  print_separator();

  fd = open(testfile, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] File creation failed!\n");
    exit(1);
  }

  printf("  Success! fd = %d\n", fd);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * Step 2: Write Data to File (first write)
   *
   * Core write path: filewrite() -> writei() -> bmap() -> log_write()
   *
   * bmap(ip, bn): Maps logical block number (bn) to physical block number
   *   - bn < NDIRECT(12): Direct block pointers (ip->addrs[bn])
   *   - bn >= NDIRECT: Mapped through indirect block
   *   - If block not yet allocated, allocate new block with balloc()
   *
   * log_write(): Does not write modified block to disk immediately; records in log
   *   - Can only be called within a transaction (begin_op/end_op)
   *   - During end_op(), commit() copies from log to actual location
   * -------------------------------------------------------- */
  printf("[Step 2] First write: write(fd, \"%s\", %d)\n",
         "Hello, xv6 file system!\\n", strlen(msg1));
  printf("  Call path:\n");
  printf("    sys_write() -> filewrite()\n");
  printf("      -> begin_op()          : Start transaction\n");
  printf("      -> ilock()             : Lock inode\n");
  printf("      -> writei()\n");
  printf("         -> bmap()           : Logical block -> physical block mapping\n");
  printf("            -> balloc()      : Allocate new data block (first write)\n");
  printf("         -> bread()          : Read block\n");
  printf("         -> either_copyin()  : Copy user data -> buffer\n");
  printf("         -> log_write()      : Schedule log write\n");
  printf("         -> iupdate()        : Update inode size\n");
  printf("      -> iunlock()           : Unlock inode\n");
  printf("      -> end_op()            : End transaction -> commit\n");
  print_separator();

  n = write(fd, msg1, strlen(msg1));
  if(n != strlen(msg1)){
    printf("  [ERROR] Write failed! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  Success! %d bytes written\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * Step 3: Write Data to File (second write)
   * Since the data block was already allocated by the first write,
   * bmap() immediately returns the existing block number without balloc().
   * The file offset (f->off) automatically advances after the first write,
   * so the second write is recorded at the next position within the same block.
   * (xv6 block size is 1024 bytes, so both messages fit in one block)
   * -------------------------------------------------------- */
  printf("[Step 3] Second write: write(fd, \"%s\", %d)\n",
         "Second write to file.\\n", strlen(msg2));
  printf("  (This time bmap() returns the already-allocated block)\n");
  print_separator();

  n = write(fd, msg2, strlen(msg2));
  if(n != strlen(msg2)){
    printf("  [ERROR] Write failed! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  Success! %d bytes written\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * Step 4: Close File
   *
   * xv6 file-related struct hierarchy:
   *   Process: p->ofile[fd] -> struct file (global file table)
   *   struct file: { type, ref, readable, writable, ip, off }
   *     - ref: Number of fds referencing this file struct (increases via fork/dup)
   *     - ip : The inode this file points to
   *     - off: Current read/write position
   *
   * On close, f->ref is decremented; when it reaches 0:
   *   - iput(ip): Decrements the inode's reference count (ref)
   *   - If inode ref is 0 and nlink is 0, itrunc() frees data blocks
   * -------------------------------------------------------- */
  printf("[Step 4] Close file: close(fd=%d)\n", fd);
  printf("  Call path:\n");
  printf("    sys_close() -> fileclose()\n");
  printf("      -> f->ref decremented (when it reaches 0):\n");
  printf("         -> begin_op()\n");
  printf("         -> iput(ip)    : Decrement inode reference\n");
  printf("         -> end_op()\n");
  print_separator();

  close(fd);
  printf("  Success! fd %d closed\n\n", fd);

  /* --------------------------------------------------------
   * Step 5: Reopen File (without O_CREATE)
   *
   * Opening without O_CREATE enters the namei() path instead of create():
   *   namei() -> namex(): Traverses each component of the path
   *     - Searches for name in each directory using dirlookup()
   *     - dirlookup(): Scans the directory's data blocks, comparing
   *       against the name field of each struct dirent
   *   ilock(): If the in-memory inode has not yet read disk contents,
   *     reads from disk via bread() (when ip->valid == 0)
   * -------------------------------------------------------- */
  printf("[Step 5] Reopen file: open(\"%s\", O_RDONLY)\n", testfile);
  printf("  Call path (no O_CREATE):\n");
  printf("    sys_open() -> namei(\"%s\")\n", testfile);
  printf("      -> namex() -> dirlookup() : Search for name in directory\n");
  printf("    -> ilock()                   : Lock inode + read from disk\n");
  printf("    -> filealloc() + fdalloc()   : Allocate file table/fd\n");
  print_separator();

  fd = open(testfile, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] File open failed!\n");
    exit(1);
  }

  printf("  Success! fd = %d\n\n", fd);

  /* --------------------------------------------------------
   * Step 6: Read File
   *
   * Core read path: readi() -> bmap() -> bread()
   *   bmap(): Converts file's logical block number to disk's physical block number
   *   bread(): Finds the block in buffer cache, or on cache miss,
   *            reads from disk and stores in cache
   *
   * Reading does not modify disk, so no transaction (begin_op/end_op) needed
   * Thanks to buffer cache, recently read blocks are returned immediately without disk I/O
   * -------------------------------------------------------- */
  printf("[Step 6] Read file: read(fd, buf, sizeof(buf))\n");
  printf("  Call path:\n");
  printf("    sys_read() -> fileread()\n");
  printf("      -> ilock()          : Lock inode\n");
  printf("      -> readi()\n");
  printf("         -> bmap()        : Logical block -> physical block\n");
  printf("         -> bread()       : Read block (check buffer cache)\n");
  printf("         -> either_copyout() : Copy buffer -> user space\n");
  printf("      -> f->off updated\n");
  printf("      -> iunlock()\n");
  printf("  (Note: read does not need a transaction - no disk modification)\n");
  print_separator();

  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, sizeof(buf) - 1);
  if(n < 0){
    printf("  [ERROR] Read failed!\n");
    close(fd);
    exit(1);
  }

  buf[n] = '\0';
  printf("  Success! %d bytes read\n", n);
  printf("  Content: \"%s\"\n\n", buf);

  /* --------------------------------------------------------
   * Step 7: Data Verification
   * -------------------------------------------------------- */
  printf("[Step 7] Data verification\n");
  print_separator();

  /* Construct full expected data */
  char expected[128];
  char *p = expected;
  /* Copy msg1 */
  char *s = msg1;
  while(*s)
    *p++ = *s++;
  /* Copy msg2 */
  s = msg2;
  while(*s)
    *p++ = *s++;
  *p = '\0';

  int expected_len = strlen(expected);

  if(n == expected_len){
    /* Byte-by-byte comparison */
    int match = 1;
    int i;
    for(i = 0; i < n; i++){
      if(buf[i] != expected[i]){
        match = 0;
        break;
      }
    }
    if(match){
      printf("  Verification succeeded! Read data matches written data.\n");
      printf("  (write -> log -> disk -> buffer cache -> read path confirmed)\n");
    } else {
      printf("  Verification failed! Data mismatch (offset %d)\n", i);
    }
  } else {
    printf("  Verification failed! Length mismatch (expected %d, got %d)\n",
           expected_len, n);
  }

  close(fd);
  printf("\n");

  /* --------------------------------------------------------
   * Step 8: Directory Creation
   *
   * A directory is a special file: an inode with type=T_DIR + directory entry data
   * When creating a new directory, two entries are automatically added:
   *   "."  : Entry pointing to itself (inum = own inode number)
   *   ".." : Entry pointing to parent directory (inum = parent's inode number)
   * The parent directory's nlink also increases by 1 (because ".." points to parent)
   *
   * sys_mkdir() -> create(path, T_DIR, 0, 0)
   *   -> ialloc + dirlink(".", ip) + dirlink("..", dp)
   * -------------------------------------------------------- */
  printf("[Step 8] Directory creation: mkdir(\"%s\")\n", testdir);
  printf("  Call path:\n");
  printf("    sys_mkdir() -> create(\"%s\", T_DIR)\n", testdir);
  printf("      -> ialloc()         : Allocate new inode (type=T_DIR)\n");
  printf("      -> dirlink(\".\")    : Self-referencing entry\n");
  printf("      -> dirlink(\"..\")   : Parent directory entry\n");
  printf("      -> dirlink(dp, \"%s\") : Register new directory in parent\n", testdir);
  printf("      -> dp->nlink++      : Increment parent's link count (due to \"..\")\n");
  print_separator();

  if(mkdir(testdir) < 0){
    printf("  [ERROR] Directory creation failed!\n");
  } else {
    printf("  Success!\n");
    print_stat(testdir);
  }
  printf("\n");

  /* --------------------------------------------------------
   * Step 9: Create File Inside Directory
   *
   * Path resolution process:
   *   The namex() function splits the path by "/" and traverses
   *   Processing "testdir/inner.txt":
   *     1. skipelem() extracts "testdir"
   *     2. dirlookup("testdir") finds the inode in current directory
   *     3. skipelem() extracts "inner.txt"
   *     4. create("inner.txt", T_FILE) is performed in testdir directory
   * -------------------------------------------------------- */
  char *nested = "testdir/inner.txt";
  printf("[Step 9] Nested file creation: open(\"%s\", O_CREATE|O_RDWR)\n", nested);
  printf("  Path resolution process (namex):\n");
  printf("    \"%s\" -> skipelem -> \"testdir\" -> dirlookup\n", nested);
  printf("                     -> skipelem -> \"inner.txt\" -> create\n");
  print_separator();

  fd = open(nested, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] Nested file creation failed!\n");
  } else {
    char *inner_msg = "nested file content\n";
    write(fd, inner_msg, strlen(inner_msg));
    printf("  Success! fd = %d\n", fd);
    print_stat(nested);
    close(fd);
  }
  printf("\n");

  /* --------------------------------------------------------
   * Step 10: Cleanup (unlink)
   *
   * unlink deletes the directory entry and decrements nlink.
   * When nlink reaches 0:
   *   - It is NOT deleted immediately!
   *   - Only when iput() finds ref==0 and nlink==0:
   *     a) itrunc(): Frees all data blocks of the file with bfree()
   *     b) iupdate(): Sets type=0 to mark the inode as free
   *   - If the file is still open (ref > 0), deletion is deferred until close
   *
   * sys_unlink() -> nameiparent() -> dirlookup() -> ip->nlink--
   * -------------------------------------------------------- */
  printf("[Step 10] File deletion: unlink(\"%s\")\n", nested);
  printf("  Call path:\n");
  printf("    sys_unlink()\n");
  printf("      -> nameiparent()  : Find parent directory\n");
  printf("      -> dirlookup()    : Find target file\n");
  printf("      -> writei()       : Zero out directory entry\n");
  printf("      -> ip->nlink--    : Decrement link count\n");
  printf("      -> iupdate()      : Write to disk\n");
  printf("  (If nlink == 0 && ref == 0, iput() frees blocks via itrunc)\n");
  print_separator();

  if(unlink(nested) < 0){
    printf("  [ERROR] Deletion failed!\n");
  } else {
    printf("  Success! \"%s\" deleted\n", nested);
  }
  printf("\n");

  /* Clean up remaining test files/directories */
  unlink(testdir);
  unlink(testfile);

  /* --------------------------------------------------------
   * Done
   * -------------------------------------------------------- */
  printf("==================================================\n");
  printf("  All file system operation tracing complete!\n");
  printf("==================================================\n");
  printf("\n");
  printf("Summary:\n");
  printf("  - File creation: sys_open -> create -> ialloc + dirlink\n");
  printf("  - File write:    sys_write -> filewrite -> writei -> log_write\n");
  printf("  - File read:     sys_read -> fileread -> readi -> bread\n");
  printf("  - Directory:     sys_mkdir -> create(T_DIR) + \".\" + \"..\"\n");
  printf("  - File deletion: sys_unlink -> nlink-- -> (itrunc if nlink==0)\n");
  printf("  - All writes are performed within begin_op/end_op transactions\n");

  exit(0);
}
