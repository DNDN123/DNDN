#!/usr/bin/env python3
"""
Crop figures from textbook page screenshots using Upstage Document Parse API.

The API detects figure elements and returns them as base64-encoded images,
along with bounding box coordinates.
"""

import sys
import os
import base64
import json
from pathlib import Path

import requests
from PIL import Image
from io import BytesIO
from dotenv import load_dotenv


def parse_document(img_path: str, api_key: str):
    """Send image to Upstage Document Parse API and get figure elements."""
    url = "https://api.upstage.ai/v1/document-digitization"
    headers = {"Authorization": f"Bearer {api_key}"}

    with open(img_path, "rb") as f:
        files = {"document": (os.path.basename(img_path), f, "image/png")}
        data = {
            "model": "document-parse",
            "base64_encoding": '["figure"]',
            "coordinates": "true",
            "output_formats": '["text"]',
        }
        resp = requests.post(url, headers=headers, files=files, data=data)

    resp.raise_for_status()
    return resp.json()


def extract_figures(result, img_path: str, output_dir: Path):
    """Extract figure elements from API response and save as images."""
    elements = result.get("elements", [])
    figures = [e for e in elements if e.get("category") == "figure"]

    if not figures:
        return []

    saved = []
    stem = Path(img_path).stem

    for i, fig in enumerate(figures):
        b64 = fig.get("base64_encoding")
        if not b64:
            # Fallback: crop using coordinates
            coords = fig.get("coordinates")
            if coords:
                saved.append(crop_with_coords(img_path, coords, output_dir, stem, i))
            continue

        # Decode base64 image
        img_data = base64.b64decode(b64)
        img = Image.open(BytesIO(img_data))

        suffix = f"_fig{i}" if len(figures) > 1 else "_fig"
        out_path = output_dir / f"{stem}{suffix}.png"
        img.save(str(out_path))
        saved.append((str(out_path), img.size))

    return saved


def crop_with_coords(img_path: str, coords, output_dir: Path, stem: str, idx: int):
    """Fallback: crop figure using normalized coordinates from API."""
    img = Image.open(img_path)
    w, h = img.size

    xs = [c["x"] for c in coords]
    ys = [c["y"] for c in coords]

    left = int(min(xs) * w)
    top = int(min(ys) * h)
    right = int(max(xs) * w)
    bottom = int(max(ys) * h)

    # Add padding
    pad = 10
    left = max(0, left - pad)
    top = max(0, top - pad)
    right = min(w, right + pad)
    bottom = min(h, bottom + pad)

    cropped = img.crop((left, top, right, bottom))
    suffix = f"_fig{idx}" if idx > 0 else "_fig"
    out_path = output_dir / f"{stem}{suffix}.png"
    cropped.save(str(out_path))
    return str(out_path), cropped.size


def process_directory(img_dir: str, output_dir: str = None):
    """Process all ch*.png files in a directory."""
    load_dotenv()
    api_key = os.getenv("UPSTAGE_API_KEY")
    if not api_key:
        print("Error: UPSTAGE_API_KEY not found in .env")
        sys.exit(1)

    img_dir = Path(img_dir)
    out_dir = Path(output_dir) if output_dir else img_dir / "cropped"
    out_dir.mkdir(parents=True, exist_ok=True)

    images = sorted(img_dir.glob("ch*.png"))
    print(f"Processing {len(images)} images...\n")

    total_figures = 0
    for img_path in images:
        try:
            result = parse_document(str(img_path), api_key)
            figures = extract_figures(result, str(img_path), out_dir)

            if figures:
                for path, size in figures:
                    print(f"  ✓ {img_path.name} → {Path(path).name} ({size[0]}x{size[1]})")
                total_figures += len(figures)
            else:
                print(f"  ✗ {img_path.name} — no figure detected")

        except requests.exceptions.HTTPError as e:
            print(f"  ✗ {img_path.name} — API error: {e}")
        except Exception as e:
            print(f"  ✗ {img_path.name} — Error: {e}")

    print(f"\nDone: {total_figures} figures extracted from {len(images)} pages")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python crop_figures.py <image_dir> [output_dir]")
        sys.exit(1)

    process_directory(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
