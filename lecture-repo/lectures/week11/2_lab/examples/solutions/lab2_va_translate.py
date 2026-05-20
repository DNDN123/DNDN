#!/usr/bin/env python3
"""
va_translate.py - RISC-V Sv39 Virtual Address Translation Visualization Tool

[Overview]
This script takes a RISC-V Sv39 virtual address as input and
visually demonstrates the 3-level page table walk process.

[Key Concept: Sv39 Page Table Structure]
RISC-V's Sv39 scheme uses 39-bit virtual addresses, translated through a 3-level page table.

  Virtual Address (39 bits):
    +----------+----------+----------+--------------+
    | VPN[2]   | VPN[1]   | VPN[0]   | Page Offset  |
    | (9 bits) | (9 bits) | (9 bits) | (12 bits)    |
    +----------+----------+----------+--------------+
    Bits 38~30   Bits 29~21  Bits 20~12   Bits 11~0

  3-Level Page Table Walk:
    1) Read the physical address of the L2 (root) page table from the satp register
    2) In the L2 table, use VPN[2] as the index to find a PTE, extract PPN to locate the L1 table
    3) In the L1 table, use VPN[1] as the index to find a PTE, extract PPN to locate the L0 table
    4) In the L0 table, use VPN[0] as the index to find a PTE, extract PPN to get the physical page base address
    5) Physical page base address + Page Offset = final physical address

  PTE (Page Table Entry) Structure (64 bits):
    Bits 63~54: Reserved
    Bits 53~10: PPN (Physical Page Number) - address of next level table or physical page
    Bits  9~ 8: RSW (reserved for software)
    Bit   7: D (Dirty) - whether a write has occurred to the page
    Bit   6: A (Accessed) - whether the page has been accessed
    Bit   5: G (Global) - global mapping
    Bit   4: U (User) - accessible from user mode
    Bit   3: X (eXecute) - executable
    Bit   2: W (Write) - writable
    Bit   1: R (Read) - readable
    Bit   0: V (Valid) - whether the PTE is valid

  xv6's PX macro (kernel/riscv.h):
    #define PX(level, va)  ((((uint64)(va)) >> (PGSHIFT + 9*(level))) & 0x1FF)
    - PX(2, va): extract VPN[2] (L2 index)
    - PX(1, va): extract VPN[1] (L1 index)
    - PX(0, va): extract VPN[0] (L0 index)

Usage:
    python3 va_translate.py                 # Interactive mode
    python3 va_translate.py 0x80001234      # Pass virtual address as argument
    python3 va_translate.py 0x1000          # User space address example
"""

import sys


# ============================================================
# Sv39 Constant Definitions
# ============================================================
# In Sv39, the page size is 4KB (2^12 = 4096 bytes)
# Each page table contains 512 PTEs (2^9 = 512, each PTE is 8 bytes)
# Therefore, a single page table is exactly one page in size (512 * 8 = 4096)
PGSIZE = 4096           # 4KB page size
PGSHIFT = 12            # Number of page offset bits (log2(4096) = 12)
LEVELS = 3              # 3-level page table (L2 -> L1 -> L0)
PTE_BITS = 9            # Index bits per level (log2(512) = 9)
MAXVA = 1 << 38         # Maximum VA used by xv6 (bit 38 is unused)
                        # Note: Sv39 uses bits 38~0 for a total of 39 bits,
                        # but xv6 defines MAXVA as 1<<38, leaving bit 38 unused

# xv6 memory layout constants (see kernel/memlayout.h)
# xv6 kernel uses direct mapping where virtual address = physical address
KERNBASE   = 0x80000000                     # Kernel code/data start address (DRAM start)
PHYSTOP    = KERNBASE + 128 * 1024 * 1024   # End of physical memory (0x88000000, 128MB)
UART0      = 0x10000000                     # Serial port I/O device address
VIRTIO0    = 0x10001000                     # VirtIO disk device address
PLIC       = 0x0C000000                     # Platform interrupt controller address
TRAMPOLINE = MAXVA - PGSIZE                 # Trampoline page (top of VA, shared by kernel/user)
TRAPFRAME  = TRAMPOLINE - PGSIZE            # Trapframe (register save area during traps)


def extract_fields(va):
    """
    Extract Sv39 fields from a virtual address.

    Sv39 virtual address structure (39 bits):
      Bits 38~30: VPN[2] (L2 index)
      Bits 29~21: VPN[1] (L1 index)
      Bits 20~12: VPN[0] (L0 index)
      Bits 11~ 0: Page Offset
    """
    offset = va & 0xFFF                    # Bits 11~0
    vpn0   = (va >> 12) & 0x1FF            # Bits 20~12
    vpn1   = (va >> 21) & 0x1FF            # Bits 29~21
    vpn2   = (va >> 30) & 0x1FF            # Bits 38~30

    return vpn2, vpn1, vpn0, offset


def format_binary(value, bits):
    """Convert a value to a binary string with the specified number of bits."""
    return format(value, f'0{bits}b')


def identify_region(va):
    """
    Determine which xv6 memory region a virtual address belongs to.

    The xv6 kernel address space mostly uses direct mapping (VA == PA).
    Exceptions: TRAMPOLINE and TRAPFRAME are mapped at the top of virtual address space
    and differ from their actual physical addresses (TRAMPOLINE maps to trampoline.S in
    kernel code, TRAPFRAME maps to a per-process allocated physical page).
    """
    if va >= TRAMPOLINE and va < TRAMPOLINE + PGSIZE:
        return "TRAMPOLINE (trap handler code)"
    elif va >= TRAPFRAME and va < TRAPFRAME + PGSIZE:
        return "TRAPFRAME (trap frame)"
    elif va >= KERNBASE and va < PHYSTOP:
        if va < KERNBASE + 0x100000:  # Approximate etext location
            return "Kernel Text - direct mapping"
        else:
            return "Kernel Data/RAM - direct mapping"
    elif va >= PLIC and va < PLIC + 0x4000000:
        return "PLIC (interrupt controller) - direct mapping"
    elif va >= UART0 and va < UART0 + PGSIZE:
        return "UART0 (serial port) - direct mapping"
    elif va >= VIRTIO0 and va < VIRTIO0 + PGSIZE:
        return "VIRTIO0 (disk I/O) - direct mapping"
    elif va < 0x10000:
        return "User process space - code/data/stack"
    elif va < KERNBASE:
        return "User process space"
    else:
        return "Unknown region"


def print_translation(va):
    """Print the detailed Sv39 translation process for a virtual address."""

    vpn2, vpn1, vpn0, offset = extract_fields(va)

    print()
    print("=" * 72)
    print(f"  RISC-V Sv39 Virtual Address Translation")
    print("=" * 72)
    print()

    # Print input virtual address
    print(f"  Input virtual address: 0x{va:016x} ({va})")
    print(f"  Region:                {identify_region(va)}")
    print()

    # VA validity check
    if va >= (1 << 39):
        print(f"  [Warning] This address exceeds 39 bits!")
        print(f"            In Sv39, bits 63~39 must be 0.")
        print(f"            xv6's MAXVA = 0x{MAXVA:x} (bit 38 is unused)")
        print()

    # 39-bit binary representation
    va39 = va & ((1 << 39) - 1)  # Extract lower 39 bits only
    binary_str = format_binary(va39, 39)

    # Field separation display
    vpn2_bin = binary_str[0:9]
    vpn1_bin = binary_str[9:18]
    vpn0_bin = binary_str[18:27]
    offset_bin = binary_str[27:39]

    print("  39-bit virtual address structure:")
    print()
    print(f"    Bits:  38       30 29       21 20       12 11            0")
    print(f"          +---------+-----------+-----------+--------------+")
    print(f"          |{vpn2_bin:^9s}|{vpn1_bin:^11s}|{vpn0_bin:^11s}|{offset_bin:^14s}|")
    print(f"          +---------+-----------+-----------+--------------+")
    print(f"           VPN[2]     VPN[1]      VPN[0]     Page Offset")
    print()

    # Detailed field information
    print("  Field analysis:")
    print(f"  ---------------------------------------------------------------")
    print(f"  VPN[2] (L2 index)  = {vpn2:>3d} (0x{vpn2:03x}, 0b{format_binary(vpn2, 9)})")
    print(f"  VPN[1] (L1 index)  = {vpn1:>3d} (0x{vpn1:03x}, 0b{format_binary(vpn1, 9)})")
    print(f"  VPN[0] (L0 index)  = {vpn0:>3d} (0x{vpn0:03x}, 0b{format_binary(vpn0, 9)})")
    print(f"  Offset             = {offset:>4d} (0x{offset:03x}, 0b{format_binary(offset, 12)})")
    print()

    # Translation process visualization
    print("  3-level page table translation process:")
    print(f"  ---------------------------------------------------------------")
    print()
    print(f"  [Step 1] satp register -> L2 table base address")
    print(f"           Read PTE at L2 table[{vpn2}]")
    print(f"           Extract PPN from PTE -> physical address of L1 table")
    print()
    print(f"  [Step 2] Read PTE at L1 table[{vpn1}]")
    print(f"           Extract PPN from PTE -> physical address of L0 table")
    print()
    print(f"  [Step 3] Read PTE at L0 table[{vpn0}]")
    print(f"           Extract PPN from PTE -> physical page base address")
    print()
    print(f"  [Final]  Physical page base address + offset(0x{offset:03x})")
    print(f"           = final physical address")
    print()

    # xv6 macro operation simulation
    print("  xv6 macro computation results:")
    print(f"  ---------------------------------------------------------------")
    print(f"  PX(2, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*2}) & 0x1FF = {vpn2}")
    print(f"  PX(1, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*1}) & 0x1FF = {vpn1}")
    print(f"  PX(0, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*0}) & 0x1FF = {vpn0}")
    print()

    # Show physical address for direct-mapped regions
    if (va >= KERNBASE and va < PHYSTOP) or \
       (va >= UART0 and va < UART0 + PGSIZE) or \
       (va >= VIRTIO0 and va < VIRTIO0 + PGSIZE) or \
       (va >= PLIC and va < PLIC + 0x4000000):
        print(f"  [Note] This address is in the kernel direct-mapped region.")
        print(f"         Physical address (PA) = Virtual address (VA) = 0x{va:016x}")
        print()

    # Virtual page information for the VA
    page_va = va & ~0xFFF  # PGROUNDDOWN
    print(f"  Virtual page information:")
    print(f"  ---------------------------------------------------------------")
    print(f"  Virtual page base address containing this VA: 0x{page_va:016x}")
    print(f"  Offset within page: {offset} bytes (0x{offset:03x})")
    print(f"  Virtual page number: {va >> 12} (0x{va >> 12:x})")
    print()


def print_example_table():
    """
    Print a table of commonly used xv6 address examples.

    For each address, compute VPN[2], VPN[1], VPN[0], and Offset to show
    which indices are used during the 3-level page table walk.
    This helps understand the relationship between xv6's memory layout and page table structure.
    """
    print()
    print("=" * 72)
    print("  xv6 Key Virtual Address Examples")
    print("=" * 72)
    print()
    print(f"  {'Address':<22s} {'L2':>5s} {'L1':>5s} {'L0':>5s} {'Offset':>6s}   Region")
    print(f"  {'-'*72}")

    examples = [
        (0x0000,      "User code start"),
        (0x1000,      "User 2nd page"),
        (UART0,       "UART0"),
        (VIRTIO0,     "VIRTIO0"),
        (PLIC,        "PLIC"),
        (KERNBASE,    "KERNBASE"),
        (KERNBASE + 0x1234, "Inside kernel text"),
        (PHYSTOP - 1, "Just before PHYSTOP"),
        (TRAPFRAME,   "TRAPFRAME"),
        (TRAMPOLINE,  "TRAMPOLINE"),
    ]

    for va, name in examples:
        vpn2, vpn1, vpn0, offset = extract_fields(va)
        print(f"  0x{va:016x}  {vpn2:>5d} {vpn1:>5d} {vpn0:>5d} 0x{offset:>03x}   {name}")

    print()


def interactive_mode():
    """
    Run in interactive mode, accepting virtual addresses from user input.

    The user can enter virtual addresses in hexadecimal (0x...) or decimal format,
    and the tool will decompose them using Sv39 and display the 3-level page table walk process.
    Exit with 'q', 'quit', 'exit', or Ctrl+C.
    """
    print()
    print("=" * 72)
    print("  RISC-V Sv39 Virtual Address Translation Tool")
    print("  (Exit: q or Ctrl+C)")
    print("=" * 72)

    # First print the example table
    print_example_table()

    while True:
        try:
            user_input = input("  Enter a virtual address (e.g., 0x80001234): ").strip()

            if user_input.lower() in ('q', 'quit', 'exit'):
                print("  Exiting.")
                break

            if user_input == '':
                continue

            # Parse input (supports 0x prefix)
            if user_input.startswith('0x') or user_input.startswith('0X'):
                va = int(user_input, 16)
            else:
                va = int(user_input)

            print_translation(va)

        except ValueError:
            print(f"  [Error] Invalid address: {user_input}")
            print(f"          Please enter a hexadecimal (0x...) or decimal value.")
            print()
        except (KeyboardInterrupt, EOFError):
            print("\n  Exiting.")
            break


def main():
    """
    Main function: if command-line arguments are given, translate those addresses;
    otherwise enter interactive mode.
    """
    if len(sys.argv) > 1:
        # Command-line argument mode: parse each argument as a virtual address and translate
        for arg in sys.argv[1:]:
            try:
                if arg.startswith('0x') or arg.startswith('0X'):
                    va = int(arg, 16)
                else:
                    va = int(arg)
                print_translation(va)
            except ValueError:
                print(f"  [Error] Invalid address: {arg}")
                sys.exit(1)

        # Also print the example table
        print_example_table()
    else:
        # Interactive mode
        interactive_mode()


if __name__ == "__main__":
    main()
