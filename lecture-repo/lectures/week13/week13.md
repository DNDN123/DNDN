# Week 13 — Storage / File System

## Overview
- **Textbook**: Silberschatz Ch 13 (File-System Interface), Ch 14 (File-System Implementation)
- **xv6 Reference**: Ch 8 (File System)
- **Learning Objectives**:
  - Understand File System Interface and Directory Structure
  - Learn File System Implementation (Allocation Methods, Free-Space Management)
  - Analyze the xv6 file system layers

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 12 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: File System Interface
  - File Concept, Access Methods
  - Directory Structure (Single-level, Two-level, Tree, Acyclic Graph)
  - File-System Mounting, File Sharing, Protection
  - Slides: `1_theory/13_storage_management_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: File System Implementation
  - File-System Structure (Layers)
  - Allocation Methods (Contiguous, Linked, Indexed)
  - Free-Space Management
  - Logging / Journaling

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:45] Lab — xv6 File System analysis
  - xv6 file system layers (buffer cache → log → inode → directory → file descriptor)
  - Logging mechanism analysis
  - fs_trace.c: File system operation tracing
  - disk_layout.py: Disk layout visualization
  - Guide: `2_lab/README.md`
- [00:45–00:50] Wrap-up + Next week project presentation reminder

## Materials
- Theory: `1_theory/13_storage_management_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (fs_trace.c, disk_layout.py)
- Homework: None
- Quiz: Yes (10 min, beginning of 1st hour)
