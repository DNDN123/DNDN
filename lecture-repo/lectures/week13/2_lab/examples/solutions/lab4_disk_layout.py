#!/usr/bin/env python3
"""
disk_layout.py - xv6 Disk Layout Visualization Script

[Overview]
Visually displays the disk block layout of the xv6 file system.
Calculations are based on constants from kernel/param.h, kernel/fs.h, and mkfs/mkfs.c.

[xv6 Disk Layout Structure]
The xv6 file system divides the disk into the following regions:

  +-------+-------+-------+-------+--------+------------------+
  | Boot  | Super | Log   | Inode | Bitmap | Data blocks      |
  | block | block | area  | area  | area   |                  |
  +-------+-------+-------+-------+--------+------------------+
  Block 0  Block 1  Block 2~  Block 33~ Block 46  Block 47~1999

  1) Boot Block (Block 0):
     - Region where the boot loader is stored (not actually used in xv6)

  2) Super Block (Block 1):
     - File system metadata: total size, size and start position of each region
     - struct superblock: { magic, size, nblocks, ninodes, nlog, logstart,
                            inodestart, bmapstart }

  3) Log Area (Block 2~32):
     - Region for WAL (Write-Ahead Logging)
     - Log header (1 block) + log data blocks (LOGBLOCKS=30)
     - Ensures crash consistency: all disk modifications are first written to the log
     - After commit, copied to actual locations (install_trans)

  4) Inode Area (Block 33~45):
     - Region where the disk inode (struct dinode) array is stored
     - Each inode is 64 bytes; one block (1024) holds 16 inodes (IPB)
     - struct dinode: { type, major, minor, nlink, size, addrs[13] }

  5) Bitmap Area (Block 46):
     - Manages used/unused status of data blocks as a bitmap
     - Each bit corresponds to one data block (1=in use, 0=free)
     - Used by balloc() to find free blocks

  6) Data Blocks (Block 47~1999):
     - Region where actual file data and directory entries are stored
     - The addrs[] array of an inode points to blocks in this region

Usage:
    python3 disk_layout.py

Educational purpose: A tool for understanding disk structure in xv6 file system lectures
"""

# ============================================================
# xv6 File System Constants (based on kernel/param.h, kernel/fs.h)
# ============================================================

# Overall file system size and block-related constants
FSSIZE = 2000           # Total file system size (in blocks, defined in mkfs/mkfs.c)
BSIZE = 1024            # Block size (bytes, BSIZE in kernel/fs.h)
                        # xv6 uses 1KB blocks (Linux etc. typically use 4KB)

# Log (WAL) related constants - settings to ensure crash consistency
MAXOPBLOCKS = 10        # Max blocks a single FS operation can write (kernel/param.h)
                        # e.g., file write involves inode block + data block + bitmap, etc.
LOGBLOCKS = MAXOPBLOCKS * 3   # Number of log data blocks (30) - allows concurrent transactions
NBUF = MAXOPBLOCKS * 3  # Buffer cache size (caches 30 blocks in memory)

# inode related constants
NINODES = 200           # Maximum number of inodes (defined in mkfs/mkfs.c)
                        # Max number of files + directories that can be created in the file system
NDIRECT = 12            # Number of direct block pointers (addrs[0]~addrs[11] in struct dinode)
BSIZE_UINT = BSIZE // 4 # Number of uints per block (1024/4 = 256 = NINDIRECT)
NINDIRECT = BSIZE_UINT  # Number of indirect block pointers (one indirect block holds 256 block addresses)

# Struct size related constants
DINODE_SIZE = 64        # sizeof(struct dinode) = short*4 + uint + uint*13 = 64 bytes
                        # type(2) + major(2) + minor(2) + nlink(2) + size(4) + addrs(52)
IPB = BSIZE // DINODE_SIZE   # Inodes per block (1024/64 = 16)
BPB = BSIZE * 8         # Bitmap bits per block (1024*8 = 8192 bits)
                        # One bitmap block can manage 8192 data blocks

DIRENT_SIZE = 16        # sizeof(struct dirent) = ushort(2) + char[14] = 16 bytes
                        # Directory entry: inode number (2 bytes) + file name (max 14 chars)


def calculate_layout():
    """
    Calculate xv6 disk layout (same logic as mkfs/mkfs.c)

    mkfs performs this calculation when creating an empty disk image (fs.img)
    to determine the start position and size of each region.
    The results are written to the superblock for the kernel to read at boot time.
    """

    nlog = LOGBLOCKS + 1          # Log header (1 block) + data blocks (30) = 31 blocks
    ninodeblocks = NINODES // IPB + 1   # 200 inodes / 16 per block + 1 = 13 blocks
    nbitmap = FSSIZE // BPB + 1         # 2000 blocks / 8192 bits + 1 = 1 block

    # Meta block total: boot(1) + super(1) + log(31) + inode(13) + bitmap(1) = 47
    nmeta = 1 + 1 + nlog + ninodeblocks + nbitmap
    nblocks = FSSIZE - nmeta            # Data blocks = 2000 - 47 = 1953

    layout = {
        "boot":     {"start": 0,            "count": 1},
        "super":    {"start": 1,            "count": 1},
        "log":      {"start": 2,            "count": nlog},
        "inode":    {"start": 2 + nlog,     "count": ninodeblocks},
        "bitmap":   {"start": 2 + nlog + ninodeblocks, "count": nbitmap},
        "data":     {"start": nmeta,        "count": nblocks},
    }

    return layout, nmeta, nblocks


def print_header():
    """Print title"""
    print("=" * 70)
    print("  xv6 File System Disk Layout Visualization")
    print("=" * 70)
    print()


def print_constants():
    """Print key constants"""
    print("[Key Constants]")
    print(f"  FSSIZE       = {FSSIZE:>6} blocks  (total file system size)")
    print(f"  BSIZE        = {BSIZE:>6} bytes   (block size)")
    print(f"  LOGBLOCKS    = {LOGBLOCKS:>6}         (number of log data blocks)")
    print(f"  NBUF         = {NBUF:>6}         (buffer cache size)")
    print(f"  NINODES      = {NINODES:>6}         (max number of inodes)")
    print(f"  NDIRECT      = {NDIRECT:>6}         (number of direct block pointers)")
    print(f"  NINDIRECT    = {NINDIRECT:>6}         (number of indirect block pointers)")
    print(f"  IPB          = {IPB:>6}         (inodes per block)")
    print(f"  BPB          = {BPB:>6}         (bitmap bits per block)")
    print(f"  MAXOPBLOCKS  = {MAXOPBLOCKS:>6}         (max write blocks per FS operation)")
    print()


def print_layout_table(layout, nmeta, nblocks):
    """Print layout table"""
    total_size_kb = FSSIZE * BSIZE / 1024

    print("[Disk Layout Details]")
    print(f"  Total size: {FSSIZE} blocks = {FSSIZE * BSIZE:,} bytes"
          f" = {total_size_kb:.0f} KB")
    print(f"  Meta blocks: {nmeta} blocks")
    print(f"  Data blocks: {nblocks} blocks")
    print()

    print(f"  {'Region':<12} {'Start Block':>10} {'Block Count':>10} "
          f"{'End Block':>10} {'Size (bytes)':>14} {'Description'}")
    print("  " + "-" * 78)

    descriptions = {
        "boot":   "Boot loader (unused in xv6)",
        "super":  "superblock (FS metadata)",
        "log":    f"WAL log (header 1 + data {LOGBLOCKS})",
        "inode":  f"Disk inodes ({NINODES} total, {IPB}/block)",
        "bitmap": f"Block usage bitmap ({BPB} bits/block)",
        "data":   "Actual file data",
    }

    for name in ["boot", "super", "log", "inode", "bitmap", "data"]:
        info = layout[name]
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        size = count * BSIZE
        desc = descriptions[name]
        print(f"  {name:<12} {start:>10} {count:>10} "
              f"{end:>10} {size:>14,} {desc}")

    print()


def print_visual_layout(layout):
    """Print visual disk map"""
    print("[Disk Block Map]")
    print()

    # Compact view
    regions = [
        ("boot",   layout["boot"],   "B"),
        ("super",  layout["super"],  "S"),
        ("log",    layout["log"],    "L"),
        ("inode",  layout["inode"],  "I"),
        ("bitmap", layout["bitmap"], "M"),
        ("data",   layout["data"],   "D"),
    ]

    # Proportional visualization (width 60 chars)
    bar_width = 60
    total = FSSIZE

    print("  Block 0" + " " * (bar_width - 13) + f"Block {FSSIZE - 1}")
    print("  |" + " " * (bar_width - 2) + "|")
    bar = ""
    labels = []
    for name, info, char in regions:
        count = info["count"]
        # Ensure minimum 1 character
        width = max(1, round(count / total * bar_width))
        bar += char * width
        labels.append((name, info["start"], char))

    # Adjust to exactly bar_width
    if len(bar) > bar_width:
        bar = bar[:bar_width]
    elif len(bar) < bar_width:
        bar = bar + "D" * (bar_width - len(bar))

    print(f"  [{bar}]")
    print()

    # Legend
    print("  Legend:")
    for name, info, char in regions:
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        pct = count / total * 100
        print(f"    {char} = {name:<8} (Block {start:>4} ~ {end:>4},"
              f" {count:>4} blocks, {pct:5.1f}%)")

    print()

    # Detailed ASCII art
    print("  Detailed Diagram:")
    print()
    print("  +------+------+---------------------------+")
    log_info = layout["log"]
    print(f"  | boot | super|         log               |")
    print(f"  |  [0] |  [1] |  [{log_info['start']}]"
          f" ~ [{log_info['start'] + log_info['count'] - 1}]"
          f"              |")
    print("  +------+------+---------------------------+")

    inode_info = layout["inode"]
    bitmap_info = layout["bitmap"]
    print(f"  |       inode blocks         | bitmap     |")
    print(f"  | [{inode_info['start']}]"
          f" ~ [{inode_info['start'] + inode_info['count'] - 1}]"
          f"               |   [{bitmap_info['start']}]      |")
    print("  +----------------------------+-----------+")

    data_info = layout["data"]
    print(f"  |                data blocks                |")
    print(f"  |  [{data_info['start']}]"
          f" ~ [{data_info['start'] + data_info['count'] - 1}]"
          f"                           |")
    print("  +-------------------------------------------+")
    print()


def print_inode_structure():
    """
    Inode structure visualization

    An inode is the core structure that stores file metadata.
    On disk it exists as struct dinode; in memory as struct inode.

    Key difference:
    - struct dinode (kernel/fs.h): 64-byte structure stored on disk
    - struct inode (kernel/file.h): Extended structure cached in memory
      Disk inode + ref (reference count) + valid (loaded from disk flag) + lock, etc.

    Block address mapping (addrs[]):
    - addrs[0]~addrs[11]: Direct block pointers -> directly reference 12 blocks
    - addrs[12]: Indirect block pointer -> indirectly reference 256 blocks
    - Total 12 + 256 = 268 blocks = 268KB is the maximum file size
    """
    print("[Inode Block Address Structure]")
    print()
    print("  struct dinode {")
    print("    short type;          // File type (2 bytes)")
    print("    short major;         // Device number (2 bytes)")
    print("    short minor;         //               (2 bytes)")
    print("    short nlink;         // Link count    (2 bytes)")
    print("    uint  size;          // File size     (4 bytes)")
    print(f"    uint  addrs[{NDIRECT}+1];  // Block addresses ({(NDIRECT+1)*4} bytes)")
    print(f"  }};  // Total {DINODE_SIZE} bytes")
    print()
    print(f"  Inodes per block = BSIZE / sizeof(dinode)"
          f" = {BSIZE} / {DINODE_SIZE} = {IPB}")
    print()

    print("  addrs[] structure:")
    print()
    print("  addrs[0]  -----> [Data Block]")
    print("  addrs[1]  -----> [Data Block]")
    print("    ...              ...")
    print(f"  addrs[{NDIRECT - 1}] -----> [Data Block]")
    print(f"                      Total {NDIRECT} direct blocks"
          f" = {NDIRECT * BSIZE:,} bytes")
    print()
    print(f"  addrs[{NDIRECT}] -----> [Indirect Block (1024 bytes)]")
    print("                     |")
    print("                     +---> addr[0]   -> [Data Block]")
    print("                     +---> addr[1]   -> [Data Block]")
    print("                     +---> ...          ...")
    print(f"                     +---> addr[{NINDIRECT - 1}] -> [Data Block]")
    print(f"                      Total {NINDIRECT} indirect blocks"
          f" = {NINDIRECT * BSIZE:,} bytes")
    print()


def print_file_size_limits():
    """
    File size limit calculation

    xv6 lab bigfile assignment: A lab exercise to extend the maximum file size
    by adding double indirect to xv6 which only has single indirect.
    This function calculates the max file size for each indirection level.
    """
    print("[File Size Limit Analysis]")
    print()

    # Current limit
    maxfile = NDIRECT + NINDIRECT
    max_bytes = maxfile * BSIZE
    print("  Current xv6 (direct + single indirect):")
    print(f"    Direct blocks:   {NDIRECT:>10} blocks")
    print(f"    Indirect blocks: {NINDIRECT:>10} blocks")
    print(f"    Total (MAXFILE):    {maxfile:>7} blocks")
    print(f"    Max file size:      {max_bytes:>7,} bytes"
          f" = {max_bytes / 1024:.0f} KB")
    print()

    # With double indirect added
    double_indirect = NINDIRECT * NINDIRECT
    maxfile_di = NDIRECT + NINDIRECT + double_indirect
    max_bytes_di = maxfile_di * BSIZE
    print("  With double indirect added:")
    print(f"    Direct blocks:            {NDIRECT:>10} blocks")
    print(f"    Single indirect blocks:   {NINDIRECT:>10} blocks")
    print(f"    Double indirect blocks: {NINDIRECT}x{NINDIRECT} = "
          f"{double_indirect:>6} blocks")
    print(f"    Total:                 {maxfile_di:>10} blocks")
    print(f"    Max file size:         {max_bytes_di:>10,} bytes"
          f" = {max_bytes_di / 1024 / 1024:.2f} MB")
    print()

    # With triple indirect added
    triple_indirect = NINDIRECT * NINDIRECT * NINDIRECT
    maxfile_ti = NDIRECT + NINDIRECT + double_indirect + triple_indirect
    max_bytes_ti = maxfile_ti * BSIZE
    print("  With triple indirect added:")
    print(f"    Direct blocks:               {NDIRECT:>12} blocks")
    print(f"    Single indirect blocks:      {NINDIRECT:>12} blocks")
    print(f"    Double indirect blocks:      {double_indirect:>12} blocks")
    print(f"    Triple indirect blocks: {NINDIRECT}^3 = {triple_indirect:>8} blocks")
    print(f"    Total:                    {maxfile_ti:>12} blocks")
    print(f"    Max file size:            {max_bytes_ti:>12,} bytes"
          f" = {max_bytes_ti / 1024 / 1024 / 1024:.2f} GB")
    print()


def print_log_structure():
    """
    Log (WAL: Write-Ahead Logging) structure visualization

    The xv6 log is the core mechanism that ensures crash consistency.

    How it works:
    1. begin_op(): Start transaction (wait if log space is insufficient)
    2. log_write(): Schedule block to be written to log (pin)
    3. end_op(): End transaction
       - If last transaction, call commit():
         a) write_log(): Copy from cache to log region
         b) write_head(): Write log header n (commit point!)
            Recovery is possible even if a crash occurs after this point
         c) install_trans(): Copy from log to original locations
         d) write_head(): Reset n=0 (clear log)

    Crash recovery (called from initlog):
    - At boot, read log header; if n > 0, re-run install_trans()
    - Only recovers already-committed transactions, ensuring atomicity
    """
    print("[Log Region Structure]")
    print()
    print(f"  MAXOPBLOCKS = {MAXOPBLOCKS}")
    print(f"  LOGBLOCKS   = MAXOPBLOCKS * 3 = {LOGBLOCKS}")
    print(f"  Log region  = header(1) + data({LOGBLOCKS}) = {LOGBLOCKS + 1} blocks")
    print()
    print("  +----------+----------+----------+-----+----------+")
    print("  | log      | log      | log      | ... | log      |")
    print("  | header   | block 0  | block 1  |     | block 29 |")
    print("  | (blk 2)  | (blk 3)  | (blk 4)  |     | (blk 32) |")
    print("  +----------+----------+----------+-----+----------+")
    print()
    print("  log header structure:")
    print("  +------+----------+----------+-----+----------+")
    print("  |  n   | block[0] | block[1] | ... |block[29] |")
    print("  |(commit| (orig   | (orig    |     | (orig    |")
    print("  | count)| loc 0)  | loc 1)   |     | loc 29)  |")
    print("  +------+----------+----------+-----+----------+")
    print()
    print("  commit() process:")
    print("    1. write_log()  : copy cache -> log region")
    print("    2. write_head() : write header.n (commit point!)")
    print("    3. install_trans(): copy log -> original locations")
    print("    4. write_head() : header.n = 0 (clear log)")
    print()

    max_write = ((MAXOPBLOCKS - 1 - 1 - 2) // 2) * BSIZE
    print(f"  Max size filewrite() can write at once:")
    print(f"    (MAXOPBLOCKS-1-1-2)/2 * BSIZE")
    print(f"    = ({MAXOPBLOCKS}-1-1-2)/2 * {BSIZE}")
    print(f"    = {(MAXOPBLOCKS - 1 - 1 - 2) // 2} * {BSIZE}")
    print(f"    = {max_write} bytes")
    print()


def print_directory_structure():
    """
    Directory entry structure visualization

    In xv6, a directory is a special file:
    - An inode with type T_DIR(1)
    - Its data blocks contain an array of struct dirent
    - Each dirent is an (inode number, filename) pair providing name->inode mapping
    - dirlookup(): Traverses directory data to search for an inode by name
    - dirlink(): Adds a new entry (finds an empty slot where inum==0 and writes)
    - File names are limited to a maximum of 14 characters (DIRSIZ=14)
    """
    print("[Directory Entry Structure]")
    print()
    print(f"  struct dirent {{")
    print(f"    ushort inum;       // Inode number (2 bytes)")
    print(f"    char   name[14];   // File name    (14 bytes)")
    print(f"  }};  // Total {DIRENT_SIZE} bytes")
    print()
    print(f"  Directory entries per block = {BSIZE} / {DIRENT_SIZE}"
          f" = {BSIZE // DIRENT_SIZE}")
    print()

    print("  Root directory (\"/\") example:")
    print("  +-------+----------------+")
    print("  | inum  | name           |")
    print("  +-------+----------------+")
    print("  |   1   | .              |")
    print("  |   1   | ..             |")
    print("  |   2   | console        |")
    print("  |   3   | init           |")
    print("  |   4   | sh             |")
    print("  |  ...  | ...            |")
    print("  +-------+----------------+")
    print()


def print_buffer_cache():
    """
    Buffer Cache (bio.c) structure visualization

    The buffer cache is a layer that caches disk blocks in memory.

    Core API:
    - bread(dev, blockno): Read block (no disk I/O on cache hit)
    - bwrite(buf): Write buffer contents to disk
    - brelse(buf): Done using buffer (decrement refcnt, move to front of LRU list)

    Data structure:
    - Doubly linked list of NBUF struct buf entries (in LRU order)
    - Each buf: { valid, disk, dev, blockno, lock, refcnt, prev, next, data[BSIZE] }
    - On cache miss in bget(), reuse the LRU (oldest) buffer

    Concurrency control:
    - bcache.lock (spinlock): Protects list structure changes and refcnt
    - buf.lock (sleeplock): Protects buffer data (can sleep during disk I/O)
      Uses sleeplock instead of spinlock because disk I/O is slow, so
      other processes should be able to use the CPU by sleeping
    """
    print("[Buffer Cache Structure]")
    print()
    print(f"  NBUF = {NBUF} (max number of blocks that can be cached)")
    print()
    print("  Doubly linked list (LRU order):")
    print()
    print("   MRU (recently used)                     LRU (oldest)")
    print("    |                                      |")
    print("    v                                      v")
    print("  head <-> buf[A] <-> buf[B] <-> ... <-> buf[Z] <-> head")
    print("       next->                               <-prev")
    print()
    print("  brelse() : If refcnt==0, move to head.next (MRU)")
    print("  bget()   : On cache miss, search from head.prev for free buffer (LRU)")
    print()
    print("  Locking scheme:")
    print("    bcache.lock (spin-lock)  : Protects list structure, refcnt")
    print("    buf.lock    (sleep-lock) : Protects buffer data (can sleep during I/O)")
    print()


def main():
    """Main function: Prints each section in order to visualize the xv6 disk structure."""
    layout, nmeta, nblocks = calculate_layout()

    print_header()
    print_constants()

    print("-" * 70)
    print()
    print_layout_table(layout, nmeta, nblocks)

    print("-" * 70)
    print()
    print_visual_layout(layout)

    print("-" * 70)
    print()
    print_inode_structure()

    print("-" * 70)
    print()
    print_file_size_limits()

    print("-" * 70)
    print()
    print_log_structure()

    print("-" * 70)
    print()
    print_directory_structure()

    print("-" * 70)
    print()
    print_buffer_cache()

    print("=" * 70)
    print("  Visualization Complete")
    print("=" * 70)


if __name__ == "__main__":
    main()
