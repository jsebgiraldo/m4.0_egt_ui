#!/usr/bin/env python3
"""Fetch a Figma node's spec via the REST API and print it as a flat table
with positions/sizes/fonts/colors already multiplied by `dt::SCALE = 1.852`
(the project's Figma→device scaling factor).

Usage:
    scripts/figma-spec.py <fileKey> <nodeId> [moreIds ...]

The token comes from ./.mcp.json (mcpServers.figma.env.FIGMA_API_KEY) or
from the FIGMA_API_KEY env var.

Output columns:
    id | type | name | bbox_figma | bbox_device | font | colour | radius
        | effects (shadow, etc.)

The bbox_device column already has SCALE applied so you can drop the value
straight into a Rect(...) literal in C++. Use it side-by-side with the
running screen to spot any field that drifted from the design.
"""

import json
import os
import sys
import time
import urllib.error
import urllib.request
from typing import Any


SCALE = 800.0 / 432.0  # 1.852 — matches src/ui/design_tokens.h::SCALE


def load_token() -> str:
    env = os.environ.get("FIGMA_API_KEY")
    if env:
        return env
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        with open(os.path.join(here, ".mcp.json")) as f:
            cfg = json.load(f)
        return cfg["mcpServers"]["figma"]["env"]["FIGMA_API_KEY"]
    except Exception as e:
        raise SystemExit(f"could not load FIGMA_API_KEY: {e}")


def api_get(token: str, path: str, max_retries: int = 14) -> dict:
    """Figma's REST rate-limit can stay hot for ~10 min after a burst.
    Total worst-case wait here = 14 × 90 s ≈ 21 min. Tune via FIGMA_RETRIES."""
    max_retries = int(os.environ.get("FIGMA_RETRIES", max_retries))
    url = f"https://api.figma.com{path}"
    req = urllib.request.Request(url, headers={"X-Figma-Token": token})
    delay = 10
    for attempt in range(max_retries):
        try:
            return json.loads(urllib.request.urlopen(req).read())
        except urllib.error.HTTPError as e:
            if e.code == 429:
                print(f"[figma-spec] 429 — retry {attempt + 1}/{max_retries} in {delay}s",
                      file=sys.stderr)
                time.sleep(delay)
                delay = min(delay * 2, 90)
                continue
            raise
    raise SystemExit("Figma API: rate-limited after retries")


def fmt_color(c: dict) -> str:
    r = int(c.get("r", 0) * 255)
    g = int(c.get("g", 0) * 255)
    b = int(c.get("b", 0) * 255)
    a = c.get("a", 1.0)
    return f"#{r:02x}{g:02x}{b:02x}" + (f"@{a:.2f}" if a < 1.0 else "")


def fmt_fills(fills: list) -> str:
    out = []
    for f in fills:
        if not f.get("visible", True):
            continue
        t = f.get("type", "?")
        if t == "SOLID":
            out.append(fmt_color(f["color"]))
        elif "GRADIENT" in t:
            stops = f.get("gradientStops", [])
            cols = ",".join(fmt_color(s["color"]) for s in stops)
            out.append(f"{t}({cols})")
        else:
            out.append(t)
    return " | ".join(out) if out else "-"


def fmt_effects(eff: list) -> str:
    out = []
    for e in eff:
        if not e.get("visible", True):
            continue
        t = e.get("type", "?")
        if "SHADOW" in t:
            off = e.get("offset", {})
            r = e.get("radius", 0)
            col = fmt_color(e.get("color", {}))
            out.append(f"{t} off=({off.get('x',0):.0f},{off.get('y',0):.0f}) "
                       f"r={r:.0f} {col}")
        else:
            out.append(t)
    return " | ".join(out) if out else "-"


def fmt_style(st: dict) -> str:
    if not st:
        return "-"
    parts = []
    fam = st.get("fontFamily")
    if fam:
        parts.append(fam)
    sz = st.get("fontSize")
    w = st.get("fontWeight")
    if sz is not None or w is not None:
        device_sz = sz * SCALE if sz else None
        parts.append(
            f"{int(sz) if sz else '?'}@{int(device_sz) if device_sz else '?'}px "
            f"w{int(w) if w else '?'}"
        )
    align = st.get("textAlignHorizontal")
    if align:
        parts.append(align.lower())
    return " ".join(parts)


def walk(node: dict, origin: tuple, indent: int = 0) -> None:
    bbox = node.get("absoluteBoundingBox") or {}
    if bbox:
        lx = bbox.get("x", 0) - origin[0]
        ly = bbox.get("y", 0) - origin[1]
        lw = bbox.get("width", 0)
        lh = bbox.get("height", 0)
        bbox_fig = f"({lx:.0f},{ly:.0f}) {lw:.0f}×{lh:.0f}"
        bbox_dev = (
            f"({lx * SCALE:.0f},{ly * SCALE:.0f}) "
            f"{lw * SCALE:.0f}×{lh * SCALE:.0f}"
        )
    else:
        bbox_fig = bbox_dev = "-"

    fills = fmt_fills(node.get("fills") or [])
    effects = fmt_effects(node.get("effects") or [])
    radius = node.get("cornerRadius")
    radius_str = f"r={int(radius)}" if radius is not None else "-"
    style = fmt_style(node.get("style") or {})

    name = (node.get("name") or "")[:24]
    typ = node.get("type", "?")
    nid = node.get("id", "?")
    txt = ""
    if typ == "TEXT":
        txt = repr(node.get("characters", "")[:30])

    pad = "  " * indent
    print(f"{pad}{nid:<14} {typ:<10} {name:<24}")
    print(f"{pad}  figma: {bbox_fig:<22} device(×SCALE): {bbox_dev}")
    print(f"{pad}  fill: {fills}   {radius_str}")
    if effects != "-":
        print(f"{pad}  effects: {effects}")
    if style != "-":
        print(f"{pad}  font:    {style}")
    if txt:
        print(f"{pad}  text:    {txt}")

    for c in node.get("children", []):
        walk(c, origin, indent + 1)


def find_node(node: dict, target_id: str):
    if node.get("id") == target_id:
        return node
    for c in node.get("children", []):
        r = find_node(c, target_id)
        if r:
            return r
    return None


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    file_key = sys.argv[1]
    ids = sys.argv[2:]
    token = load_token()

    # Strategy: cache the whole file at depth=4 (one request) instead of
    # hitting /v1/files/.../nodes (which rate-limits very aggressively).
    # /v1/files/{key}?depth=N has a separate, looser limit, so this is the
    # only way to reliably extract specs once we've burned the /nodes quota.
    cache = f"/tmp/figma-spec-cache-{file_key}.json"
    if os.environ.get("FIGMA_NO_CACHE") or not os.path.exists(cache):
        print(f"[figma-spec] fetching /v1/files/{file_key}?depth=4 ...",
              file=sys.stderr)
        data = api_get(token, f"/v1/files/{file_key}?depth=4")
        with open(cache, "w") as f:
            json.dump(data, f)
    else:
        print(f"[figma-spec] using cache {cache} "
              f"(set FIGMA_NO_CACHE=1 to refresh)", file=sys.stderr)
        with open(cache) as f:
            data = json.load(f)

    for nid in ids:
        node = find_node(data["document"], nid)
        if not node:
            print(f"\n══════ {nid} ── NOT FOUND ──────────")
            continue
        bbox = node.get("absoluteBoundingBox") or {}
        origin = (bbox.get("x", 0), bbox.get("y", 0))
        print(f"\n══════ {nid} ── {node.get('name','')} ──────────")
        walk(node, origin)
    return 0


if __name__ == "__main__":
    sys.exit(main())
