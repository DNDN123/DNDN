# SmartShell Host Bridge

Natural-language front-end that drives xv6-riscv running in QEMU.

## Quick start

```bash
# Easiest: drop the key into a local .env file (gitignored)
echo 'UPSTAGE_API_KEY=up_...' > .env

# OR export it manually
export UPSTAGE_API_KEY=up_...

# Optional override of the xv6 source path
export XV6_DIR=/path/to/xv6-riscv

python3 nl_bridge.py
```

The bridge auto-loads `host/.env` on startup (existing shell env vars win).
Without any key set, it falls back to a tiny rule-based parser (offline dev).
The QEMU driver is real either way — boots xv6 and pipes commands over stdin.

## Supported intents (W10 MVP)

| Intent   | NL example                  | xv6 command         |
|----------|-----------------------------|---------------------|
| PS       | "show me the processes"     | `ps`                |
| SETPRIO  | "set pid 4 priority to 3"   | `priority_test` †   |
| SPAWN    | "run priority_test"         | `priority_test`     |
| KILL     | "kill 5"                    | `kill 5`            |
| REJECT   | (anything unsafe)           | (nothing)           |

† Direct `setprio` user program isn't in the tree yet — the bridge falls
back to running `priority_test` for now. To be replaced when a `setprio.c`
ships.

## Safety guard

- `KILL` of pid 0 or 1 (init) is refused
- `SPAWN` is whitelisted (`SPAWN_WHITELIST` in `nl_bridge.py`)
- `SETPRIO` validates `0 <= prio <= 20`

## Known issues

- xv6 currently prints `[sched]/[sleep]/[wakeup]` debug lines on every
  scheduling event. The bridge filters these out via `_NOISE_RE`, but they
  should be moved behind a `#ifdef SCHED_DEBUG` in `kernel/proc.c`.
- The QEMU driver uses `time.sleep(settle)` to wait for output. A pty-based
  driver with proper prompt detection is more robust — TODO before W12.
