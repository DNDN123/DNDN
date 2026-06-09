"""Verify nlos.py resolves the on-device (local) backend in `auto` mode."""
import argparse
import nlos

print("DEFAULT_MODEL      :", nlos.DEFAULT_MODEL)
print("have_local_model() :", nlos.have_local_model(nlos.DEFAULT_MODEL))
print("torch_ok()         :", nlos.torch_ok())
print("has_real_key()     :", nlos.has_real_key())

args = argparse.Namespace(backend="auto", model=nlos.DEFAULT_MODEL, port=11434)
chosen = nlos.choose_backend(args)
print("choose_backend(auto) ->", chosen)
assert chosen == "local", f"expected 'local', got {chosen!r}"
print("OK: nlos.py picks the on-device local backend.")
