# Lab 2: Sv39 Virtual Address Translation (va_translate.py)

## Description

`lab2_va_translate.py` is a Python script that visualizes RISC-V Sv39 virtual address translation. Given a 64-bit virtual address, it decomposes the lower 39 bits into the three page-table index fields (VPN[2], VPN[1], VPN[0]) and the 12-bit page offset, then prints a detailed diagram of the 3-level translation process.

The script also identifies which xv6 memory region the address belongs to (kernel text, UART, TRAMPOLINE, user space, etc.) and shows how the xv6 `PX()` macro computes each index.

This exercise accompanies the manual `walk()` tracing questions in Exercise 2 of the lab worksheet.

## How to Use

Run the script with Python 3. No external libraries are required.

```bash
# Interactive mode: enter addresses one at a time
python3 lab2_va_translate.py

# Command-line mode: pass one or more addresses as arguments
python3 lab2_va_translate.py 0x80001234
python3 lab2_va_translate.py 0x0000 0x10000000 0x80000000
```

In interactive mode the script first prints a table of common xv6 addresses, then prompts for input. Type `q` to quit.

## What to Observe

1. **Bit-field diagram** -- The 39-bit virtual address is split into four labeled fields (VPN[2], VPN[1], VPN[0], Offset) shown in both binary and decimal.
2. **3-step translation walkthrough** -- The output traces how the hardware would use satp to reach the L2 table, then follow PTE chains through L1 and L0 to the final physical page.
3. **xv6 macro calculations** -- The script prints the exact `PX(2, va)`, `PX(1, va)`, `PX(0, va)` results so you can verify your manual calculations.
4. **Region identification** -- Each address is mapped to its xv6 memory region (e.g., KERNBASE direct-mapped area, TRAMPOLINE, user space).
5. **Direct-mapping note** -- For kernel direct-mapped regions the script reminds you that PA == VA.

## Discussion Questions

1. For the address `0x80001234`, what are VPN[2], VPN[1], VPN[0], and the page offset? Verify your manual calculation against the script output.
2. Why does the TRAMPOLINE address (`MAXVA - PGSIZE`) have VPN[2] = 255 and VPN[1] = 511, VPN[0] = 511?
3. Two user-space addresses `0x0000` and `0x1000` share the same VPN[2] and VPN[1] values but differ in VPN[0]. What does this tell you about how many L0 table entries a small user process needs?
4. The script caps valid addresses at 39 bits and warns about larger values. Why does Sv39 ignore bits 63-39, and what constraint does xv6 impose with `MAXVA = 1 << 38`?
5. If you wanted to extend this script to perform a full translation (producing the final physical address), what additional information would you need beyond the virtual address itself?
