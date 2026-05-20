#!/bin/bash
# apply_patches.sh — Smart-MLFQ-xv6 패치를 강의용 xv6 소스 트리에 적용
#
# 사용법:
#   ./apply_patches.sh <path-to-xv6-riscv>
#
# 예시:
#   ./apply_patches.sh ~/2026-lecture-operating-system/xv6-riscv
#   ./apply_patches.sh ../lecture-repo/xv6-riscv

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <path-to-xv6-riscv>" >&2
  echo "" >&2
  echo "Example: $0 ../lecture-repo/xv6-riscv" >&2
  exit 1
fi

XV6="$1"
PATCH_DIR="$(cd "$(dirname "$0")" && pwd)"

# Sanity check
if [ ! -d "$XV6/kernel" ] || [ ! -d "$XV6/user" ] || [ ! -f "$XV6/Makefile" ]; then
  echo "ERROR: $XV6 doesn't look like an xv6-riscv tree" >&2
  echo "  (expected kernel/, user/, Makefile)" >&2
  exit 1
fi

# Sanity check on patch directory
for f in kernel/proc.c kernel/proc.h kernel/trap.c kernel/syscall.c \
         kernel/syscall.h kernel/sysproc.c kernel/defs.h \
         user/user.h user/usys.pl; do
  if [ ! -f "$PATCH_DIR/$f" ]; then
    echo "ERROR: $PATCH_DIR/$f missing" >&2
    exit 1
  fi
done

# Backup originals before overwrite
BACKUP="$XV6/.backup-pre-mlfq-$(date +%Y%m%d-%H%M%S)"
echo "==> Creating backup at $BACKUP"
mkdir -p "$BACKUP/kernel" "$BACKUP/user"
for f in kernel/proc.c kernel/proc.h kernel/trap.c kernel/syscall.c \
         kernel/syscall.h kernel/sysproc.c kernel/defs.h \
         user/user.h user/usys.pl; do
  if [ -f "$XV6/$f" ]; then
    cp "$XV6/$f" "$BACKUP/$f"
  fi
done

# Apply patches
echo "==> Copying kernel patches"
cp -v "$PATCH_DIR"/kernel/*.c "$PATCH_DIR"/kernel/*.h "$XV6/kernel/"

echo "==> Copying user patches"
cp -v "$PATCH_DIR"/user/user.h  "$XV6/user/"
cp -v "$PATCH_DIR"/user/usys.pl "$XV6/user/"

# Workload + nl-shell user programs (cpu_burner, io_burner, wrunner, nlrun)
echo "==> Copying workload user programs"
for f in cpu_burner.c io_burner.c mixed_burner.c wrunner.c nlrun.c diagprog.c; do
  if [ -f "$PATCH_DIR/user/$f" ]; then
    cp -v "$PATCH_DIR/user/$f" "$XV6/user/"
  fi
done

# Patch UPROGS in Makefile (idempotent — checks if entry already present)
echo "==> Patching Makefile UPROGS"
for prog in cpu_burner io_burner mixed_burner wrunner nlrun diagprog; do
  if ! grep -q "\\\$U/_$prog" "$XV6/Makefile"; then
    sed -i "/^UPROGS=\\\\$/,/^$/ { /_dorphan\\\\$/a\\
\\	\$U/_$prog\\\\
}" "$XV6/Makefile"
    echo "    + added \$U/_$prog to UPROGS"
  else
    echo "    = \$U/_$prog already in UPROGS"
  fi
done

# Copy workload .txt files into xv6 build dir + register in fs.img target.
echo "==> Copying workload text files"
PATCH_WL="$PATCH_DIR/workloads"
if [ -d "$PATCH_WL" ]; then
  for wl in cpu_heavy.txt io_heavy.txt mixed.txt three_way.txt realprog.txt hints_example.txt; do
    if [ -f "$PATCH_WL/$wl" ]; then
      cp -v "$PATCH_WL/$wl" "$XV6/$wl"
    fi
  done
  # `hints.txt` starts empty so baseline runs have a no-op hints file.
  if [ ! -f "$XV6/hints.txt" ]; then
    echo "# empty (baseline — no hints)" > "$XV6/hints.txt"
    echo "    + created empty hints.txt"
  fi
fi

echo "==> Registering .txt files in fs.img"
WL_TXTS="cpu_heavy.txt io_heavy.txt mixed.txt three_way.txt realprog.txt hints.txt"
for wl in $WL_TXTS; do
  if ! grep -q "fs.img:.*$wl" "$XV6/Makefile"; then
    # Append to the fs.img dependency + mkfs recipe.
    sed -i "s|^\(fs\.img:[^\n]*\)$|\1 $wl|" "$XV6/Makefile"
    sed -i "s|^\(	mkfs/mkfs fs\.img[^\n]*\)$|\1 $wl|" "$XV6/Makefile"
    echo "    + added $wl to fs.img target"
  else
    echo "    = $wl already in fs.img"
  fi
done

echo ""
echo "==> Done. Patches applied to $XV6"
echo ""
echo "Next steps:"
echo "  cd $XV6"
echo "  make clean"
echo "  make qemu"
echo ""
echo "To revert: copy files from $BACKUP back."
