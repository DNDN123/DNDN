#!/usr/bin/env python3
"""model_server — serve YOUR fine-tuned SLM behind an OpenAI-compatible endpoint.

This is the "on-device" backend for the NL bridge. It loads the LoRA adapter you
trained (ml/models/smartmlfq-qwen-3b-r64-e10/lora) on top of its base
(Qwen2.5-3B-Instruct) and exposes:

    POST /v1/chat/completions     (the exact shape executor.classify() calls)

So the whole stack runs locally, no Solar, no network:

    # terminal 1 — serve the model you trained
    cd host && python3 model_server.py
    # terminal 2 — xv6 + bridge, pointed at the local model
    cd host && UPSTAGE_BASE_URL=http://127.0.0.1:11434/v1 UPSTAGE_MODEL=smartmlfq \
               python3 nlbridge.py

Loading/prompting mirror ml/scripts/ask.py exactly so the served model behaves
identically to training. The system prompt is NOT hardcoded here — it arrives in
each request from executor.SYSTEM_PROMPT, keeping a single source of truth.

First run downloads the base model (~6GB) from HuggingFace. Apple Silicon uses
MPS automatically. Deps:  pip install torch transformers peft accelerate
"""
from __future__ import annotations
import os
# Let any op MPS doesn't implement fall back to CPU instead of crashing
# (harmless on CUDA/CPU). Must be set before torch is imported.
os.environ.setdefault("PYTORCH_ENABLE_MPS_FALLBACK", "1")
import re
import json
import argparse
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_MODEL = os.path.join(REPO, "ml", "models", "smartmlfq-qwen-3b-r64-e10", "lora")

_tok = None
_model = None
_gen_lock = threading.Lock()      # model.generate is not thread-safe; serialize


def _from_pretrained(cls, model_id, dtype):
    """transformers >=5 uses `dtype=`, <5 uses `torch_dtype=`. Support both."""
    try:
        return cls.from_pretrained(model_id, dtype=dtype)
    except TypeError:
        return cls.from_pretrained(model_id, torch_dtype=dtype)


def load(model_path: str):
    """Load base + LoRA (or a merged model). Mirrors ask.py, MPS-aware."""
    global _tok, _model
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    if torch.cuda.is_available():
        device = "cuda"
        dtype = torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
    elif getattr(torch.backends, "mps", None) and torch.backends.mps.is_available():
        device = "mps"
        dtype = torch.float16
    else:
        device = "cpu"
        dtype = torch.float32

    print(f"[model_server] loading {model_path}")
    print(f"[model_server] device={device} dtype={dtype}")

    if os.path.isfile(os.path.join(model_path, "adapter_config.json")):
        from peft import PeftConfig, PeftModel
        base_id = PeftConfig.from_pretrained(model_path).base_model_name_or_path
        print(f"[model_server] base = {base_id} (first run downloads ~6GB)")
        _tok = AutoTokenizer.from_pretrained(base_id)
        base = _from_pretrained(AutoModelForCausalLM, base_id, dtype)
        _model = PeftModel.from_pretrained(base, model_path)
    else:
        _tok = AutoTokenizer.from_pretrained(model_path)
        _model = _from_pretrained(AutoModelForCausalLM, model_path, dtype)

    _model.to(device)
    _model.eval()
    if _tok.pad_token is None:
        _tok.pad_token = _tok.eos_token
    print("[model_server] ready.")


def generate(messages, max_new_tokens=256) -> str:
    import torch
    prompt = _tok.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
    inputs = _tok(prompt, return_tensors="pt").to(_model.device)
    with _gen_lock, torch.no_grad():      # serialize: generate() is not reentrant
        out = _model.generate(**inputs, max_new_tokens=max_new_tokens,
                              do_sample=False, pad_token_id=_tok.eos_token_id)
    raw = _tok.decode(out[0][inputs.input_ids.shape[1]:], skip_special_tokens=True).strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    return raw


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):            # quiet default logging
        pass

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.rstrip("/") == "/v1/models":
            self._json(200, {"object": "list",
                             "data": [{"id": "smartmlfq", "object": "model"}]})
        else:
            self._json(404, {"error": "not found"})

    def do_POST(self):
        if self.path.rstrip("/") != "/v1/chat/completions":
            self._json(404, {"error": "not found"})
            return
        n = int(self.headers.get("Content-Length", 0))
        try:
            req = json.loads(self.rfile.read(n) or b"{}")
            messages = req.get("messages", [])
            text = generate(messages, int(req.get("max_tokens", 256)))
        except Exception as e:
            self._json(500, {"error": f"{type(e).__name__}: {e}"})
            return
        self._json(200, {
            "id": "chatcmpl-local",
            "object": "chat.completion",
            "model": req.get("model", "smartmlfq"),
            "choices": [{"index": 0, "finish_reason": "stop",
                         "message": {"role": "assistant", "content": text}}],
        })


def main():
    import sys
    try:
        sys.stdout.reconfigure(line_buffering=True)   # logs/readiness show promptly
    except Exception:
        pass
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default=DEFAULT_MODEL)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=11434)
    args = ap.parse_args()

    load(args.model)
    srv = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"[model_server] OpenAI-compatible endpoint at "
          f"http://{args.host}:{args.port}/v1/chat/completions  (Ctrl-C to stop)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n[model_server] bye")


if __name__ == "__main__":
    main()
