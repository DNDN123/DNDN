# xv6 Development Environment Setup Guide

This guide explains how to set up the development environment for building and running **xv6-riscv** on your machine.

## Required Tools

| Tool | Purpose |
|------|---------|
| **RISC-V GCC toolchain** | Cross-compiler for building xv6 (RISC-V target) |
| **QEMU** | Emulator to run xv6 on your x86/ARM machine |
| **git** | Clone the xv6 repository |
| **make** | Build system |

---

## macOS

### Install with Homebrew

```bash
# Install Homebrew (skip if already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install QEMU
brew install qemu

# Install RISC-V cross-compiler
brew install riscv64-elf-gcc

# Verify installation
qemu-system-riscv64 --version
riscv64-elf-gcc --version
```

### Clone and Build xv6

```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make qemu
```

If the xv6 shell prompt (`$`) appears, the setup is complete. Press `Ctrl-A` then `X` to exit QEMU.

---

## Ubuntu / Debian

### Install with apt

```bash
sudo apt update
sudo apt install -y git build-essential

# Install QEMU (RISC-V system emulator)
sudo apt install -y qemu-system-misc

# Install RISC-V cross-compiler
sudo apt install -y gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu

# Verify installation
qemu-system-riscv64 --version
riscv64-linux-gnu-gcc --version
```

> **Note**: On older Ubuntu versions (< 22.04), the package names may differ.
> If `gcc-riscv64-linux-gnu` is not available, use `gcc-riscv64-unknown-elf` or
> build from source (see Troubleshooting below).

### TOOLPREFIX Configuration

The xv6 Makefile auto-detects the toolchain prefix. If it fails, set it manually:

```bash
# If your compiler is riscv64-linux-gnu-gcc
export TOOLPREFIX=riscv64-linux-gnu-

# Build and run
cd xv6-riscv
make qemu
```

### Clone and Build xv6

```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make qemu
```

Press `Ctrl-A` then `X` to exit QEMU.

---

## Windows (WSL2)

Windows users should use **WSL2** (Windows Subsystem for Linux) with an Ubuntu distribution.

### Step 1: Install WSL2

Open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu
```

Restart your computer when prompted. After reboot, Ubuntu will launch and ask you to create a username and password.

### Step 2: Install Tools inside WSL2

Once inside the WSL2 Ubuntu terminal, follow the Ubuntu instructions above:

```bash
sudo apt update
sudo apt install -y git build-essential qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu

# Verify
qemu-system-riscv64 --version
riscv64-linux-gnu-gcc --version
```

### Step 3: Clone and Build xv6

```bash
# IMPORTANT: Work inside the Linux filesystem, NOT /mnt/c/
# The /mnt/c/ path is very slow due to filesystem translation.
cd ~
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make qemu
```

Press `Ctrl-A` then `X` to exit QEMU.

### Tips for WSL2

- **Use the Linux filesystem** (`~/` or `/home/username/`), not `/mnt/c/`. Building on `/mnt/c/` is significantly slower.
- **VS Code integration**: Install the "WSL" extension in VS Code, then run `code .` from the WSL terminal to edit files.
- If QEMU graphics fail, ensure you are using `make qemu` (not `make qemu-gdb`), which uses `-nographic` mode by default in xv6.

---

## Verifying Your Setup

After installation, run this quick test:

```bash
cd xv6-riscv
make clean
make qemu
```

You should see output like:

```
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

Try these commands in the xv6 shell:

```
$ ls
$ echo hello
$ cat README
```

Exit QEMU: press `Ctrl-A`, then `X`.

---

## Troubleshooting

### "Couldn't find a riscv64 version of GCC/binutils"

The Makefile cannot detect your cross-compiler. Set `TOOLPREFIX` manually:

```bash
# Check what's installed
ls /usr/bin/riscv64-*
# or
ls /opt/homebrew/bin/riscv64-*

# Set the prefix accordingly
make TOOLPREFIX=riscv64-linux-gnu- qemu
# or
make TOOLPREFIX=riscv64-elf- qemu
```

### QEMU version too old

xv6-riscv requires QEMU 5.0+. Check your version:

```bash
qemu-system-riscv64 --version
```

If it's below 5.0, install from source or use a newer package repository.

### Build errors on macOS with Apple Silicon

If you see linker errors, ensure you installed `riscv64-elf-gcc` (not the Linux GNU variant):

```bash
brew install riscv64-elf-gcc
```

### WSL2: "KVM not available"

This warning is harmless. xv6 QEMU runs in software emulation mode (TCG) and does not require KVM.

---

## References

- [MIT 6.1810 Tools Page](https://pdos.csail.mit.edu/6.828/2024/tools.html)
- [xv6-riscv GitHub Repository](https://github.com/mit-pdos/xv6-riscv)
