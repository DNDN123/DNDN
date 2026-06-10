"""train_tiny.py — Train a TINY on-device intent classifier that runs INSIDE xv6.

Unlike the 3B LoRA model (which needs torch + 6GB and cannot fit in xv6's
128MB guest), this is a hashed-ngram linear classifier:

  text --(FNV-1a hashed byte n-grams)--> sparse int features
       --(20-class linear model, integer weights)--> argmax intent

Training runs here on the host in PURE PYTHON (no numpy/torch). The learned
weights are quantized to int16 and emitted as a C header
(os/user/nlmodel_weights.h) so the SAME math runs natively inside xv6 with
pure integer arithmetic — no floating point (sidesteps xv6's lack of FP
context-switch save), no network, no host bridge.

The feature extraction here MUST stay byte-for-byte identical to the C
implementation in os/user/nlmodel.c (same FNV-1a, same n-gram sizes, same
lowercase rule, same bucket count D).

Usage:
  python3 train_tiny.py
  python3 train_tiny.py --epochs 25 --dim 4096
"""
import argparse
import json
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TRAIN = os.path.join(ROOT, "ml", "data", "train.jsonl")
TEST = os.path.join(ROOT, "ml", "data", "test.jsonl")
HEADER_OUT = os.path.join(ROOT, "os", "user", "nlmodel_weights.h")

# 20-intent label set, in a FIXED canonical order (index = class id).
# Must match the order written into the C header.
CLASSES = [
    "cpu_burner", "io_burner", "mixed_burner", "echo",
    "ls", "cat", "rm", "mkdir", "ln",
    "ps", "kill", "setpri", "trace",
    "killall", "killheavy", "reap",
    "uptime", "sysinfo", "explain", "reject",
]
CLASS_ID = {c: i for i, c in enumerate(CLASSES)}
NCLASS = len(CLASSES)

# --- Feature extraction (KEEP IN SYNC WITH nlmodel.c) ---------------------
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
NGRAMS = (3, 4, 5)   # byte n-gram sizes


def _lower_bytes(text):
    """UTF-8 bytes with ASCII A-Z folded to a-z. Non-ASCII bytes untouched."""
    out = bytearray(text.encode("utf-8"))
    for i, b in enumerate(out):
        if 65 <= b <= 90:          # 'A'..'Z'
            out[i] = b + 32
    return bytes(out)


def features(text, dim):
    """Return dict bucket -> count of hashed byte n-grams."""
    b = _lower_bytes(text)
    L = len(b)
    feats = {}
    for n in NGRAMS:
        if L < n:
            continue
        for i in range(0, L - n + 1):
            h = (FNV_OFFSET ^ n) & 0xFFFFFFFF
            for j in range(i, i + n):
                h ^= b[j]
                h = (h * FNV_PRIME) & 0xFFFFFFFF
            bucket = h % dim
            feats[bucket] = feats.get(bucket, 0) + 1
    return feats


# --- Data ------------------------------------------------------------------
def load(path, dim):
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            o = json.loads(line)
            cmd = o["spec"]["cmd"]
            if cmd not in CLASS_ID:
                continue
            rows.append((features(o["user"], dim), CLASS_ID[cmd]))
    return rows


# --- Training: multiclass softmax via SGD ---------------------------------
def train(rows, dim, epochs, lr, l2, seed=0):
    rng = random.Random(seed)
    # W[c] is a flat list of length dim; bias per class.
    W = [[0.0] * dim for _ in range(NCLASS)]
    bias = [0.0] * NCLASS
    idx = list(range(len(rows)))
    for ep in range(epochs):
        rng.shuffle(idx)
        cur_lr = lr / (1.0 + 0.3 * ep)
        loss_sum = 0.0
        for k in idx:
            feats, y = rows[k]
            items = list(feats.items())
            # scores
            scores = [bias[c] for c in range(NCLASS)]
            for c in range(NCLASS):
                wc = W[c]
                s = scores[c]
                for b, v in items:
                    s += wc[b] * v
                scores[c] = s
            m = max(scores)
            exps = [math.exp(s - m) for s in scores]
            Z = sum(exps)
            probs = [e / Z for e in exps]
            loss_sum += -math.log(max(probs[y], 1e-12))
            # gradient step
            for c in range(NCLASS):
                g = probs[c] - (1.0 if c == y else 0.0)
                if g == 0.0 and not items:
                    pass
                wc = W[c]
                glr = cur_lr * g
                if glr != 0.0:
                    for b, v in items:
                        wc[b] -= glr * v + cur_lr * l2 * wc[b]
                    bias[c] -= glr
        acc = evaluate(rows, W, bias)
        print(f"  epoch {ep+1:2d}/{epochs}  loss={loss_sum/len(rows):.4f}  train_acc={acc:.3f}")
    return W, bias


def predict(feats, W, bias):
    best_c, best_s = 0, None
    items = list(feats.items())
    for c in range(NCLASS):
        wc = W[c]
        s = bias[c]
        for b, v in items:
            s += wc[b] * v
        if best_s is None or s > best_s:
            best_s, best_c = s, c
    return best_c


def evaluate(rows, W, bias):
    ok = 0
    for feats, y in rows:
        if predict(feats, W, bias) == y:
            ok += 1
    return ok / len(rows) if rows else 0.0


# --- Quantize to int16 + emit C header ------------------------------------
def quantize(W, bias):
    maxabs = 1e-9
    for c in range(NCLASS):
        for w in W[c]:
            if abs(w) > maxabs:
                maxabs = abs(w)
    for b in bias:
        if abs(b) > maxabs:
            maxabs = abs(b)
    scale = 30000.0 / maxabs            # keep within int16 range
    Wq = [[int(round(w * scale)) for w in W[c]] for c in range(NCLASS)]
    bq = [int(round(b * scale)) for b in bias]
    return Wq, bq, scale


def eval_quant(rows, Wq, bq):
    ok = 0
    for feats, y in rows:
        best_c, best_s = 0, None
        items = list(feats.items())
        for c in range(NCLASS):
            wc = Wq[c]
            s = bq[c]
            for b, v in items:
                s += wc[b] * v
            if best_s is None or s > best_s:
                best_s, best_c = s, c
        if best_c == y:
            ok += 1
    return ok / len(rows) if rows else 0.0


def emit_header(Wq, bq, dim):
    lines = []
    lines.append("// AUTO-GENERATED by ml/scripts/train_tiny.py — do not edit by hand.")
    lines.append("// Tiny on-device intent classifier weights (int16, hashed byte n-grams).")
    lines.append("#ifndef NLMODEL_WEIGHTS_H")
    lines.append("#define NLMODEL_WEIGHTS_H")
    lines.append("")
    lines.append(f"#define NL_DIM {dim}")
    lines.append(f"#define NL_NCLASS {NCLASS}")
    lines.append(f"#define NL_FNV_OFFSET {FNV_OFFSET}u")
    lines.append(f"#define NL_FNV_PRIME {FNV_PRIME}u")
    ng = ",".join(str(n) for n in NGRAMS)
    lines.append(f"#define NL_NGRAMS {{{ng}}}")
    lines.append(f"#define NL_NNGRAM {len(NGRAMS)}")
    lines.append("")
    names = ", ".join(f'"{c}"' for c in CLASSES)
    lines.append(f"static const char *nl_class_name[NL_NCLASS] = {{ {names} }};")
    lines.append("")
    lines.append("static const short nl_bias[NL_NCLASS] = {")
    lines.append("  " + ", ".join(str(v) for v in bq))
    lines.append("};")
    lines.append("")
    lines.append("static const short nl_w[NL_NCLASS][NL_DIM] = {")
    for c in range(NCLASS):
        row = ",".join(str(v) for v in Wq[c])
        lines.append(f"  {{ {row} }},")
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    with open(HEADER_OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    sz = os.path.getsize(HEADER_OUT)
    print(f"  wrote {HEADER_OUT} ({sz} bytes)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--dim", type=int, default=4096)
    ap.add_argument("--lr", type=float, default=0.5)
    ap.add_argument("--l2", type=float, default=1e-6)
    args = ap.parse_args()

    print(f"Loading data (dim={args.dim}) ...")
    train_rows = load(TRAIN, args.dim)
    test_rows = load(TEST, args.dim)
    print(f"  train={len(train_rows)}  test={len(test_rows)}  classes={NCLASS}")

    print("Training (pure-python softmax SGD) ...")
    W, bias = train(train_rows, args.dim, args.epochs, args.lr, args.l2)

    tr = evaluate(train_rows, W, bias)
    te = evaluate(test_rows, W, bias)
    print(f"float   model:  train_acc={tr:.3f}  test_acc={te:.3f}")

    Wq, bq, scale = quantize(W, bias)
    trq = eval_quant(train_rows, Wq, bq)
    teq = eval_quant(test_rows, Wq, bq)
    print(f"int16   model:  train_acc={trq:.3f}  test_acc={teq:.3f}  (scale={scale:.2f})")

    emit_header(Wq, bq, args.dim)
    print("Done.")


if __name__ == "__main__":
    main()
