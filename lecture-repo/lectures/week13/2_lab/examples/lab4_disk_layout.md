# Lab 4: Large File Support (disk_layout.py)

## Description

`lab4_disk_layout.py` is a Python visualization script that calculates and
displays the xv6 file system's on-disk layout. It uses the same constants as
`kernel/param.h`, `kernel/fs.h`, and `mkfs/mkfs.c` to reproduce the exact block
allocation computed by `mkfs`.

The script visualizes:

- **Disk layout**: Block ranges for boot, superblock, log, inode, bitmap, and
  data regions, with a proportional ASCII bar chart.
- **Inode structure**: The `dinode` struct layout showing direct and indirect
  block pointers.
- **File size limits**: Current maximum (268 KB with single indirect), and
  projected maximums with double indirect (~64 MB) and triple indirect (~16 GB).
- **Log structure**: WAL log header and data block layout, plus the 4-stage
  commit process.
- **Directory entries**: The `dirent` structure and an example root directory.
- **Buffer cache**: LRU doubly-linked list structure and locking scheme.

## How to Use

This is a standalone Python script. No xv6 build is required.

```bash
# Run directly (requires Python 3)
python3 lab4_disk_layout.py
```

The script produces text output to the terminal. No external libraries are
needed.

### Experimenting with Different Parameters

You can edit the constants at the top of the script to explore how changes
affect the disk layout:

```python
FSSIZE = 2000       # Try increasing to 10000
NINODES = 200       # Try increasing to 1000
MAXOPBLOCKS = 10    # Try changing to 15
```

After changing a value, re-run the script to see the updated layout and file
size calculations.

## What to Observe

- **Block allocation**: Note that metadata (boot + super + log + inode + bitmap)
  occupies 46 blocks out of 2000. The remaining 1954 blocks are available for
  data. This means roughly 97.7% of the disk is usable for file data.
- **Log sizing**: `LOGBLOCKS = MAXOPBLOCKS * 3 = 30`. The factor of 3 allows
  multiple concurrent file system operations to share the log. The log header
  takes one additional block.
- **Inode density**: Each inode is 64 bytes, so 16 inodes fit in a single 1024-
  byte block. With 200 inodes, 13 blocks are needed (200/16 + 1).
- **File size limits**: The jump from single indirect (268 KB) to double indirect
  (~64 MB) is dramatic -- a factor of ~245x. This shows why multi-level
  indirection is essential for supporting large files.
- **Write size constraint**: `filewrite` can only write
  `(MAXOPBLOCKS-1-1-2)/2 * BSIZE = 3072 bytes` per transaction. Large writes
  are split across multiple transactions.

## Discussion Questions

1. If `FSSIZE` is increased to 10,000 blocks, how many bitmap blocks would be
   needed? (Hint: `BPB = BSIZE * 8 = 8192 bits per block`.)
2. The current `MAXFILE` is 268 blocks (268 KB). Is this enough for a typical
   xv6 user program binary? What about storing a large dataset?
3. Why does the log area need `MAXOPBLOCKS * 3` blocks instead of just
   `MAXOPBLOCKS`? What would happen if the log were too small?
4. If you wanted to support files larger than 64 MB without adding triple
   indirection, what alternative design choices could you make? (Consider
   changing `BSIZE`, using extents, etc.)
5. The buffer cache has only 30 entries (`NBUF = 30`). Given that the file
   system has 2000 blocks, what is the cache hit ratio you would expect under
   heavy load? How would increasing `NBUF` affect performance?
