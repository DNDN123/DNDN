#!/usr/bin/env python3
"""nlos — one command to talk to xv6 in natural language.

Picks a translation backend, makes sure it is reachable, then launches the NL
bridge (xv6 + console glue). The xv6 part needs a Unix-like environment:
macOS, Linux, or WSL2 on Windows. The model backend itself can be Solar (cloud)
or your fine-tuned model running locally (Mac MPS / Windows+CUDA / CPU).

    python3 nlos.py                   # auto: solar if key, else local model, else offline
    python3 nlos.py --backend local   # force your on-device fine-tuned model
    python3 nlos.py --backend solar   # force Upstage Solar Pro 3  (needs UPSTAGE_API_KEY)
    python3 nlos.py --backend offline # deterministic rules, no LLM (demo with no key/model)

Backend selection is the ONLY thing that changes between Solar and on-device —
the bridge, guard, kernel and xv6 commands are identical either way.
"""
from __future__ import annotations
import argparse
import os
import sys
import time
import subprocess
import urllib.request

from nlbridge import load_env       # shared .env loader (no python-dotenv dep)

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DEFAULT_MODEL = os.path.join(REPO, "ml", "models", "smartmlfq-qwen-3b-r64-e10", "lora")

# Treat unset / template / sentinel values as "no real Solar key".
_PLACEHOLDER_KEYS = {"", "your_api_key_here", "not-needed", "local"}


def has_real_key() -> bool:
    for var in ("UPSTAGE_API_KEY", "SOLAR_API_KEY"):
        if os.environ.get(var, "").strip() not in _PLACEHOLDER_KEYS:
            return True
    return False


def endpoint_up(base: str) -> bool:
    try:
        with urllib.request.urlopen(base.rstrip("/") + "/models", timeout=2) as r:
            return r.status == 200
    except Exception:
        return False


def have_local_model(path: str) -> bool:
    if os.path.isfile(os.path.join(path, "adapter_config.json")):
        return True
    try:                                       # listdir can fail (TOCTOU, perms)
        return any(f.endswith(".safetensors") for f in os.listdir(path))
    except OSError:
        return False


def torch_ok() -> bool:
    try:
        import torch  # noqa: F401
        return True
    except Exception:
        return False


def choose_backend(args) -> str:
    if args.backend != "auto":
        return args.backend
    if has_real_key():
        return "solar"
    if have_local_model(args.model) and torch_ok():
        return "local"
    return "offline"


def main() -> int:
    ap = argparse.ArgumentParser(description="Talk to xv6 in natural language.")
    ap.add_argument("--backend", choices=["auto", "local", "solar", "offline"], default="auto")
    ap.add_argument("--model", default=DEFAULT_MODEL, help="LoRA/merged model dir for local backend")
    ap.add_argument("--port", type=int, default=11434, help="local model server port")
    args = ap.parse_args()

    load_env()                       # pick up host/.env before auto-detection
    backend = choose_backend(args)
    env = dict(os.environ)
    server = None

    if backend == "solar":
        env.setdefault("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
        env.setdefault("UPSTAGE_MODEL", "solar-pro3")
        if not has_real_key():
            print("[nlos] backend=solar but no real UPSTAGE_API_KEY / SOLAR_API_KEY set "
                  "(check host/.env — placeholder values are ignored).")
            return 2
        print("[nlos] backend = Solar Pro 3 (cloud)")

    elif backend == "local":
        if not have_local_model(args.model):
            print(f"[nlos] no local model at {args.model}")
            print("[nlos] train it (ml/scripts/train.py) or use --backend solar/offline.")
            return 2
        if not torch_ok():
            print("[nlos] torch not installed — pip install -r host/requirements-local.txt")
            return 2
        base = f"http://127.0.0.1:{args.port}/v1"
        if endpoint_up(base):
            print(f"[nlos] reusing model server already running at {base}")
        else:
            print("[nlos] starting on-device model server "
                  "(first run downloads the base model, ~6GB) ...")
            logpath = os.path.join(HERE, "model_server.out")
            with open(logpath, "w") as logf:      # child dups the fd; close ours
                server = subprocess.Popen(
                    [sys.executable, os.path.join(HERE, "model_server.py"),
                     "--model", args.model, "--port", str(args.port)],
                    stdout=logf, stderr=subprocess.STDOUT,
                )
            for _ in range(2400):                 # up to ~40 min for first download
                if server.poll() is not None:
                    print(f"[nlos] model server exited early — see {logpath}")
                    return 2
                if endpoint_up(base):
                    break
                time.sleep(1)
            else:
                print("[nlos] model server did not become ready in time.")
                if server.poll() is None:
                    server.terminate()
                return 2
        env["UPSTAGE_BASE_URL"] = base
        env["UPSTAGE_MODEL"] = "smartmlfq"
        env.setdefault("SOLAR_API_KEY", "local")  # executor needs a non-empty key
        print(f"[nlos] backend = on-device fine-tuned model ({base})")

    else:  # offline
        env["NLBRIDGE_OFFLINE"] = "1"
        print("[nlos] backend = offline rules (no LLM, deterministic)")

    print("[nlos] handing off to the NL bridge ...\n")
    try:
        return subprocess.run(
            [sys.executable, os.path.join(HERE, "nlbridge.py")],
            cwd=HERE, env=env,
        ).returncode
    finally:
        if server is not None and server.poll() is None:
            print("\n[nlos] stopping model server ...")
            server.terminate()
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()


if __name__ == "__main__":
    raise SystemExit(main())
