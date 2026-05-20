# Week 1 Lab — Coding Agents Hands-on

## Overview

In this lab, you will install and use a **coding agent** — an AI-powered CLI tool that can read, write, and execute code autonomously. These tools will be used throughout the semester for labs and assignments.

---

## Available Coding Agents

### Free Tier

| Agent | Provider | Notes |
|-------|----------|-------|
| **Gemini CLI** | Google | Free tier: **1000 calls/day** |

### Pro (Paid / Licensed)

| Agent | Provider | Notes |
|-------|----------|-------|
| **Claude Code** | Anthropic | CLI-based, agentic coding |
| **GPT Codex** | OpenAI | CLI-based |

### Open-Source Harness

| Agent | Notes |
|-------|-------|
| **OpenCode** | Use any model (including open-source LLMs) |
| | https://github.com/anomalyco/opencode |
| **oh-my-opencode** | OpenCode wrapper |
| | https://github.com/code-yeongyu/oh-my-opencode |

---

## Task 1 — Install

Install at least one coding agent by following its official documentation.

**Gemini CLI (recommended for free tier):**
```bash
npm install -g @anthropic-ai/claude-code   # Claude Code
# or
npm install -g @google/gemini-cli           # Gemini CLI
```

Verify installation:
```bash
claude --version   # or gemini --version
```

---

## Task 2 — Organize Files

Use the agent to **organize files** in your Downloads folder or another messy directory.

Example prompt:
> "Organize the files in ~/Downloads by file type into subfolders (images, documents, code, etc.)"

---

## Task 3 — Documentation

Use the agent to **document a GitHub repository**.

Pick any public repo, or use:
- https://github.com/code-yeongyu/oh-my-opencode

Example prompt:
> "Read this codebase and generate a comprehensive README.md with architecture overview, setup instructions, and usage examples."

---

## Task 4 — RALPH Technique

Create a **verifiable evaluation rubric** and use it to iteratively improve output quality.

1. Ask the agent to generate a rubric (e.g., "Top-tier ML conference review criteria")
2. Ask the agent to evaluate its own output against the rubric
3. Keep iterating: "Improve until all criteria are met"

Key phrases to try:
- "Keep going until the criteria are met"
- "Evaluate against the rubric and fix all issues"

---

## Task 5 — Make an OS

Use the agent to **build a minimal OS**.

Example prompt:
> "Create a minimal bootable OS for x86 that prints 'Hello, OS!' to the screen. Include a Makefile and instructions to run it in QEMU."

This is a preview of the **final project** — in Week 9, you'll form teams and build an OS prototype using these agents.

---

## Submission

No submission required. Explore freely and get comfortable with the tools.
