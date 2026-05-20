#!/usr/bin/env python3
"""
Extract figures from textbook chapter PDFs using Upstage Document Parse API.

Pipeline: Chapter PDF → page images → Upstage API → cropped figures
"""

import sys
import os
import base64
import tempfile
from pathlib import Path
from io import BytesIO

import fitz  # PyMuPDF
import requests
from PIL import Image
from dotenv import load_dotenv


def pdf_to_page_images(pdf_path: str, dpi: int = 200):
    """Extract each page of a PDF as a PNG image."""
    doc = fitz.open(pdf_path)
    pages = []
    for i, page in enumerate(doc):
        mat = fitz.Matrix(dpi / 72, dpi / 72)
        pix = page.get_pixmap(matrix=mat)
        img_data = pix.tobytes("png")
        pages.append((i + 1, img_data))
    doc.close()
    return pages


def detect_figures(img_data: bytes, api_key: str, filename: str = "page.png"):
    """Send a page image to Upstage Document Parse API."""
    url = "https://api.upstage.ai/v1/document-digitization"
    headers = {"Authorization": f"Bearer {api_key}"}

    files = {"document": (filename, img_data, "image/png")}
    data = {
        "model": "document-parse",
        "base64_encoding": '["figure"]',
        "coordinates": "true",
        "output_formats": '["text"]',
    }
    resp = requests.post(url, headers=headers, files=files, data=data)
    resp.raise_for_status()
    return resp.json()


def extract_chapter_figures(pdf_path: str, output_dir: str, api_key: str):
    """Extract all figures from a chapter PDF."""
    pdf_path = Path(pdf_path)
    out_dir = Path(output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    chapter_name = pdf_path.stem  # e.g., "Ch03_Processes"
    print(f"\n{'='*60}")
    print(f"Processing: {chapter_name}")
    print(f"{'='*60}")

    # Extract pages
    pages = pdf_to_page_images(str(pdf_path))
    print(f"  Pages: {len(pages)}")

    figures = []
    for page_num, img_data in pages:
        try:
            result = detect_figures(img_data, api_key, f"p{page_num:03d}.png")
            elements = result.get("elements", [])
            page_figures = [e for e in elements if e.get("category") == "figure"]

            for i, fig in enumerate(page_figures):
                b64 = fig.get("base64_encoding")
                if not b64:
                    continue

                # Decode and save
                decoded = base64.b64decode(b64)
                img = Image.open(BytesIO(decoded))

                # Get caption if available
                captions = [e for e in elements if e.get("category") == "caption"]
                caption_text = ""
                if captions:
                    # Find caption closest to this figure (by page position)
                    for cap in captions:
                        cap_text = cap.get("content", {}).get("text", "")
                        if cap_text:
                            caption_text = cap_text
                            break

                suffix = f"_fig{i}" if len(page_figures) > 1 else ""
                fname = f"p{page_num:03d}{suffix}.png"
                out_path = out_dir / fname
                img.save(str(out_path))

                figures.append({
                    "file": fname,
                    "page": page_num,
                    "size": img.size,
                    "caption": caption_text[:100],
                })
                print(f"  ✓ Page {page_num} → {fname} ({img.size[0]}x{img.size[1]}) {caption_text[:60]}")

        except Exception as e:
            print(f"  ✗ Page {page_num} — Error: {e}")

    if not figures:
        print("  No figures found!")
    else:
        print(f"\n  Total: {len(figures)} figures extracted")

    return figures


# Chapter-to-week mapping
CHAPTER_WEEK_MAP = {
    "Ch03_Processes": ["week02", "week03"],
    "Ch04_Threads_and_Concurrency": ["week04", "week05"],
    "Ch05_CPU_Scheduling": ["week06", "week07"],
    "Ch06_Synchronization_Tools": ["week09"],
    "Ch07_Synchronization_Examples": ["week09"],
    "Ch08_Deadlocks": ["week10"],
    "Ch09_Main_Memory": ["week11"],
    "Ch10_Virtual_Memory": ["week12"],
    "Ch11_Mass-Storage_Structure": ["week13"],
}


if __name__ == "__main__":
    load_dotenv()
    api_key = os.getenv("UPSTAGE_API_KEY")
    if not api_key:
        print("Error: UPSTAGE_API_KEY not found")
        sys.exit(1)

    books_dir = Path("materials/books/operating_system_concepts")
    lectures_dir = Path("lectures")

    # Process specific chapter or all
    if len(sys.argv) > 1:
        chapters = sys.argv[1:]
    else:
        chapters = list(CHAPTER_WEEK_MAP.keys())

    for chapter in chapters:
        pdf_file = books_dir / f"{chapter}.pdf"
        if not pdf_file.exists():
            print(f"  ✗ {pdf_file} not found, skipping")
            continue

        # Determine output directory
        weeks = CHAPTER_WEEK_MAP.get(chapter, [])
        if weeks:
            # Put in first week's images/figures dir
            out_dir = lectures_dir / weeks[0] / "1_theory" / "images" / "figures"
        else:
            out_dir = Path("figures") / chapter

        extract_chapter_figures(str(pdf_file), str(out_dir), api_key)
