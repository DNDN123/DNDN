---
theme: default
title: "Week 13 — Lab: File System Internals"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 13 — File System Internals

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

- **Goal**: Analyze the xv6 file system layer by layer, trace file operations end-to-end
- **Duration**: ~50 minutes · 4 exercises
- **Reference**: `kernel/bio.c`, `kernel/log.c`, `kernel/fs.c`, `kernel/sysfile.c`

```mermaid
graph LR
    E1["Ex 1<br/>Buffer Cache"] --> E2["Ex 2<br/>Logging"]
    E2 --> E3["Ex 3<br/>File Creation"]
    E3 --> E4["Ex 4<br/>Large Files"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
    style E4 fill:#fce4ec
```

---

# xv6 File System — 6 Layers

```mermaid
graph TD
    L6["6. File Descriptors<br/>open(), read(), write(), close()"] --> L5
    L5["5. Pathnames<br/>namei(), nameiparent()"] --> L4
    L4["4. Directories<br/>dirlookup(), dirlink()"] --> L3
    L3["3. Inodes<br/>ialloc(), iget(), readi(), writei()"] --> L2
    L2["2. Logging<br/>begin_op(), log_write(), end_op()"] --> L1
    L1["1. Buffer Cache<br/>bread(), bwrite(), brelse()"] --> DISK["Block Device<br/>(virtio disk)"]
    style L6 fill:#e3f2fd
    style L5 fill:#bbdefb
    style L4 fill:#c8e6c9
    style L3 fill:#fff3e0
    style L2 fill:#fce4ec
    style L1 fill:#f3e5f5
    style DISK fill:#eeeeee
```

- Each layer relies **only** on the layer below
- **Logging** provides crash safety
- **Buffer cache** ensures one in-memory copy per disk block

---

# Exercise 1: Buffer Cache

**Purpose**: Cache disk blocks in memory; serialize access with sleep-locks

**Key functions in `kernel/bio.c`:**

| Function | Role |
|---|---|
| `binit()` | Initialize doubly-linked LRU list |
| `bget()` | Return cached block or evict LRU entry |
| `bread()` | `bget()` + read from disk if not valid |
| `bwrite()` | Write buffer to disk |
| `brelse()` | Release buffer; move to MRU end |

```mermaid
graph LR
    REQ["bread(dev, blockno)"] --> BG["bget()"]
    BG --> HIT{"Cache hit?"}
    HIT -->|Yes| RET["Return buffer ✅"]
    HIT -->|No| EVICT["Evict LRU<br/>(refcnt == 0)"]
    EVICT --> READ["Read from disk"]
    READ --> RET
    style HIT fill:#fff3e0
    style RET fill:#c8e6c9
    style EVICT fill:#ffcdd2
```

- `buf.refcnt == 0` → evictable
- Each buffer has a **sleeplock** — one process at a time

---

# Exercise 2: Logging System

**Write-ahead logging (WAL)** — guarantees atomic multi-block updates across crashes

```mermaid
graph TD
    BO["begin_op()<br/>increment op count;<br/>wait if log full"] --> LW["log_write(b)<br/>record block number;<br/>pin buffer"]
    LW --> EO["end_op()<br/>decrement count"]
    EO --> C{"Last op?"}
    C -->|Yes| CM["commit()"]
    CM --> WL["write_log()<br/>blocks → log region"]
    WL --> WH["write_head()<br/>commit point ✅"]
    WH --> IT["install_trans()<br/>log → real locations"]
    IT --> CL["write_head(0)<br/>clear log"]
    C -->|No| DONE["Wait for others"]
    style WH fill:#c8e6c9
    style CM fill:#fff3e0
```

**Crash recovery** (`recover_from_log()` at boot):
- Committed header found → re-run `install_trans()`
- No committed header → discard log, disk is consistent

---

# Exercise 3: File Creation Tracing

**Full call chain for `open("newfile", O_CREATE)`:**

```mermaid
graph TD
    SO["sys_open()<br/>kernel/sysfile.c"] --> CR["create()"]
    CR --> NP["nameiparent()<br/>resolve parent dir"]
    CR --> DL["dirlookup()<br/>check name doesn't exist"]
    CR --> IA["ialloc()<br/>allocate new inode"]
    IA --> LW1["log_write()"]
    CR --> DK["dirlink()<br/>add entry to parent"]
    DK --> WI["writei()"]
    WI --> LW2["log_write()"]
    CR --> FA["filealloc()<br/>+ fdalloc()"]
    FA --> RET["Return fd to user"]
    style SO fill:#e3f2fd
    style IA fill:#fff3e0
    style DK fill:#e8f5e9
    style RET fill:#c8e6c9
```

**Task**: Set a breakpoint at `ialloc()` in GDB and verify the inode type and device number are set correctly before `log_write()`.

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab3_fs_trace.c</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab3_fs_trace.c</code></div>

---

# Exercise 4: Large File Support

**xv6 inode block layout** (`struct dinode` in `kernel/fs.h`):

```mermaid
graph TD
    I["struct dinode"] --> D["addrs[0..11]<br/>12 direct blocks<br/>= 12 KB"]
    I --> S["addrs[12]<br/>single indirect<br/>256 blocks = 256 KB"]
    I --> DB["addrs[13]<br/>double indirect<br/>256² = 64 MB"]
    I --> TR["addrs[14]<br/>triple indirect<br/>256³ = 16 GB"]
    S --> S1["256 block ptrs"]
    DB --> DB1["256 indirect ptrs"]
    DB1 --> DB2["256 block ptrs each"]
    style D fill:#c8e6c9
    style S fill:#e3f2fd
    style DB fill:#fff3e0
    style TR fill:#fce4ec
```

| Level | Formula | Max Size |
|---|---|---|
| Direct | 12 blocks | 12 KB |
| Single indirect | 256 blocks | 256 KB |
| Double indirect | 256 × 256 | 64 MB |
| Triple indirect | 256³ | 16 GB |

**Task**: Modify `bmap()` in `kernel/fs.c` to support **double-indirect** blocks.

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab4_disk_layout.py</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab4_disk_layout.py</code></div>

---

# Key Takeaways

```mermaid
graph LR
    BC["Buffer Cache<br/>LRU + sleep-locks"] --> LOG["Logging<br/>WAL crash safety"]
    LOG --> FS["File Ops<br/>ialloc + dirlink"]
    FS --> LF["Large Files<br/>indirect blocks"]
    style BC fill:#f3e5f5
    style LOG fill:#fce4ec
    style FS fill:#e8f5e9
    style LF fill:#fff3e0
```

| Concept | Key Insight |
|---|---|
| **Buffer cache** | One copy per block; LRU eviction; sleep-locks |
| **Logging** | `write_head()` = commit point; recovery replays committed txns |
| **File creation** | Touches inode block + parent dir block — both wrapped in a transaction |
| **Large files** | Indirect blocks multiply capacity: 256/block → each level adds ×256 |

> The file system is the most complex xv6 subsystem. Reading it **layer by layer** is the most effective approach.
