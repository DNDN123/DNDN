# Planning & Decision Archive

Historical planning artefacts kept for traceability. **Not load-bearing
for the final submission** — the actual implementation lives in
`smart-mlfq-host/` (Python host tools) and `smart-mlfq-xv6-patches/`
(xv6 kernel patches).

## What's here

| File | Status | Note |
|---|---|---|
| `decision-aid-A-vs-B.md` | superseded | A vs B direction decision matrix |
| `team-pitch-direction-A.md` | **not chosen** | Direction A (OS for LLM) team pitch |
| `weekly-roadmap-direction-A.md` | **not chosen** | Direction A weekly plan |
| `B-주제-탐색.md` | superseded | Direction B sub-theme exploration |
| `MLFQ-roadmap.md` | superseded | Early MLFQ scoping |
| `Solo-MLFQ-roadmap.md` | superseded | Solo-version Python sim plan |
| `Solo-MLFQ-xv6-roadmap.md` | **partially applied** | xv6 solo plan — most of this got built |
| `Week09-첫단계.md` | superseded | W9 kickoff notes |
| `BUILD_FROM_SCRATCH.md` | reference | Setup walk-through |

## Why archived, not deleted

- Direction A docs document the chosen / not-chosen decision
- Solo-MLFQ-xv6-roadmap.md is the closest spec to what actually shipped
- Snippets inside still reference older model IDs (e.g. `solar-pro2`) —
  those are frozen snapshots of past planning, not the running config.
  Current config: `smart-mlfq-host/.env.example`.

For active documentation, see:
- `smart-mlfq-host/docs/architecture-scheduler.md`
- `smart-mlfq-host/docs/W11-slides.md`
- `smart-mlfq-host/DEMO_W11.md`
