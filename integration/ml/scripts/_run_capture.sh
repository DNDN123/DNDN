#!/usr/bin/env bash
cd "/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/integration/ml/scripts" || exit 1
PY="/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/integration/host/venv/bin/python"
"$PY" capture_ondevice.py 2>&1 | grep -v "it/s\]" | tee ondevice_demo_transcript.txt
