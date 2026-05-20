#!/usr/bin/env python3
"""Extract figures from textbook PDF using Upstage Document Parse API."""

import os
import sys
import json
import base64
import tempfile
import requests
import fitz  # PyMuPDF
from pathlib import Path
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env")

API_KEY = os.getenv("UPSTAGE_API_KEY")
API_URL = "https://api.upstage.ai/v1/document-digitization"
PDF_PATH = Path(__file__).resolve().parent.parent / "materials/books/operating_system_concepts/Ch03_Processes.pdf"
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "lectures/week03/1_theory/images/figures"


def extract_pdf_pages(pdf_path, page_start, page_end):
    """Extract specific pages from PDF into a temporary file."""
    doc = fitz.open(pdf_path)
    new_doc = fitz.open()
    # page_start and page_end are 1-indexed
    for i in range(page_start - 1, min(page_end, len(doc))):
        new_doc.insert_pdf(doc, from_page=i, to_page=i)

    tmp = tempfile.NamedTemporaryFile(suffix=".pdf", delete=False)
    new_doc.save(tmp.name)
    new_doc.close()
    doc.close()
    return tmp.name


def call_upstage_api(pdf_path):
    """Call Upstage Document Parse API to extract figures."""
    headers = {"Authorization": f"Bearer {API_KEY}"}

    with open(pdf_path, "rb") as f:
        files = {"document": ("extract.pdf", f, "application/pdf")}
        data = {
            "base64_encoding": '["figure"]',
            "output_formats": '["html"]',
            "model": "document-parse",
        }

        print(f"  Calling Upstage API...")
        resp = requests.post(API_URL, headers=headers, files=files, data=data, timeout=180)

    if resp.status_code != 200:
        print(f"  Error {resp.status_code}: {resp.text[:500]}")
        return None

    return resp.json()


def save_figures(result, prefix, page_offset=0):
    """Save base64-encoded figures from API response."""
    if not result:
        return []

    saved = []
    elements = result.get("elements", [])

    for i, elem in enumerate(elements):
        cat = elem.get("category", "")
        page = elem.get("page", 0)
        actual_page = page + page_offset

        if cat == "figure" and elem.get("base64_encoding"):
            b64 = elem["base64_encoding"]
            img_data = base64.b64decode(b64)

            filename = f"{prefix}_p{actual_page:03d}.png"
            filepath = OUTPUT_DIR / filename
            with open(filepath, "wb") as f:
                f.write(img_data)

            size_kb = len(img_data) / 1024
            print(f"  Saved: {filename} (page {actual_page}, {size_kb:.1f} KB)")
            saved.append(filepath)

    # Show all detected categories for debugging
    cats = {}
    for elem in elements:
        cat = elem.get("category", "unknown")
        cats[cat] = cats.get(cat, 0) + 1
    print(f"  Element categories: {cats}")

    return saved


def main():
    if not API_KEY:
        print("Error: UPSTAGE_API_KEY not found in .env")
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Extract key pages with diagrams
    # PDF page 20 = textbook p124 (Chrome multiprocess architecture)
    # PDF page 21 = textbook p125 (Figure 3.11 - IPC models)
    # PDF page 35 = textbook p139 (Figure 3.19 - ALPC)
    # PDF page 36 = textbook p140 (Figure 3.20 - Pipe fd)
    # PDF page 43 = textbook p147 (Figure 3.26 - Sockets)
    # PDF page 48 = textbook p152 (Figure 3.29 - RPC)

    page_groups = [
        ("chrome_ipc", 20, 21, 19),      # Chrome + IPC models
        ("alpc_pipe", 35, 36, 34),        # ALPC + Pipe diagram
        ("socket_rpc", 43, 48, 42),       # Socket + RPC diagrams
    ]

    all_saved = []
    for prefix, start, end, offset in page_groups:
        print(f"\n=== Extracting pages {start}-{end} ({prefix}) ===")
        tmp_pdf = extract_pdf_pages(PDF_PATH, start, end)
        try:
            result = call_upstage_api(tmp_pdf)
            saved = save_figures(result, prefix, page_offset=offset)
            all_saved.extend(saved)
        finally:
            os.unlink(tmp_pdf)

    print(f"\n=== Total figures extracted: {len(all_saved)} ===")
    for f in all_saved:
        print(f"  {f.name}")
    print("\nDone!")


if __name__ == "__main__":
    main()
