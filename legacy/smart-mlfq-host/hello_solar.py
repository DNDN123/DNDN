"""hello_solar.py — verify Solar API key works."""
import os
import sys

from dotenv import load_dotenv
from openai import OpenAI


def main():
    load_dotenv()
    key = os.getenv("UPSTAGE_API_KEY")
    base = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
    model = os.getenv("UPSTAGE_MODEL", "solar-pro3")
    effort = os.getenv("UPSTAGE_REASONING_EFFORT", "low")

    if not key or key == "your_api_key_here":
        print("ERROR: set UPSTAGE_API_KEY in .env")
        return 1

    print(f"[hello] model={model} effort={effort} base={base}")
    client = OpenAI(api_key=key, base_url=base)
    resp = client.chat.completions.create(
        model=model,
        messages=[{"role": "user", "content": "Say the single word 'ok'."}],
        temperature=0.0,
        max_tokens=512,
        extra_body={"reasoning_effort": effort},
    )
    print(f"[hello] response: {resp.choices[0].message.content!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
