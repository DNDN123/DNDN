# Repository Policy — Canonical Source & GitHub Submission

> Last reviewed: 2026-05-20 (W12 end). Read this once and the cross-platform
> bugs we kept hitting in W11 should stay dead.

## 1. Single source of truth

**Canonical tree (edit here):**
```
C:\Users\Hss\Documents\Claude\Projects\OS Project\
```
This is the only place where source files are *authored*. Everything else
(WSL working copies, GitHub) is a derivative.

**Derived locations (do not author here):**
- WSL execution targets:
  - `/root/smart-mlfq-host/`        (Python host scripts run here)
  - `/root/xv6-riscv/`              (xv6 build + QEMU run here)
  - `/root/smart-mlfq-xv6-patches/` (patch staging — overwritten by sync)
  - `/root/xv6-stock/`              (stock xv6 build for RR comparison)
- GitHub: `<TEAM_REPO_URL>` (public, mirrors the canonical tree)

Sync direction is **always one way: canonical → derived**. If a change is
made in WSL by accident, copy it back to Windows immediately and then
re-sync downward.

## 2. Why this matters

In W11 we hit three sync incidents that cost real time:
1. `viz.py` ended up with two different versions; the WSL one had a
   markdown paste artefact (leading 2-space indent on every line)
2. `.env` was created as a directory on WSL by a misformatted `cp`
3. `apply_patches.sh` initially only copied kernel files, not workload
   `.c` files — fresh xv6 trees came up without `_nlrun`

Each is the same root cause: editing in two places without a sync
contract.

## 3. Approved sync command (W12+)

```bash
# Windows-side files → WSL working tree
wsl -d Ubuntu -- bash -c '
SRC="/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project"
cp -r "$SRC/smart-mlfq-host/"*.py        /root/smart-mlfq-host/
cp -r "$SRC/smart-mlfq-host/"*.md        /root/smart-mlfq-host/
cp -r "$SRC/smart-mlfq-host/docs/"       /root/smart-mlfq-host/
cp -r "$SRC/smart-mlfq-host/run_w12_matrix.sh" /root/smart-mlfq-host/
cp -r "$SRC/smart-mlfq-xv6-patches/"     /root/smart-mlfq-xv6-patches/
# .env is gitignored — only re-sync if you explicitly want to overwrite
'
```

## 4. GitHub submission checklist

Before pushing to the public repo:

- [ ] `git check-ignore .env` returns successfully (i.e. .env is ignored)
- [ ] `git log -p | grep -i 'UPSTAGE_API_KEY=up_'` returns nothing
      (no real key has ever been committed)
- [ ] `.env.example` contains placeholder text, **never a real key**
- [ ] Repository is set to **public** in GitHub Settings → General
- [ ] Top-level `README.md` covers: one-paragraph summary, setup,
      how-to-run, demo screenshots/video link
- [ ] `smart-mlfq-host/docs/architecture-scheduler.md` is rendered
      correctly on GitHub (Mermaid block renders, OS-concept table
      is legible)
- [ ] `smart-mlfq-host/docs/technical-report.md` has zero `[TODO]`
      placeholders
- [ ] `smart-mlfq-host/docs/W11-slides.md` and `W14-slides.md` are
      pushed
- [ ] Charts directory (`charts_w11/`, latest W12 `*/charts/`) is
      committed so they render in the README

## 5. Key rotation cadence

The Solar Pro 3 API key is renewable from
<https://console.upstage.ai/api-keys>. **Rotate it whenever any of these
happens:**
- The key was pasted into a chat / email / conversation log
- A teammate's machine was lost or compromised
- A push accidentally included `.env`
- More than 4 weeks have passed since the last rotation

## 6. Re-discovering this document

If a future contributor (or future you) is confused about *which*
directory is real:
```bash
grep -r "Canonical source of truth" .
```
The top-level `README.md` will surface this policy.
