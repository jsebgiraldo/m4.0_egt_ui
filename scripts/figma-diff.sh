#!/usr/bin/env bash
# Pixel-level Figma vs simulator diff workflow.
#
#   ./scripts/figma-diff.sh <fileKey> <nodeId> <reportDir>
#
# Pulls the Figma render as `figma.png`, expects you to have captured a
# matching `after.png` (via scripts/screenshot-sim.sh) into the same
# reportDir, and produces:
#
#   figma.png       — Figma render at 2× (downloaded fresh each run)
#   diff.png        — pixel-diff overlay (red pixels = mismatch)
#   compare.png     — 3-up [Figma | After | Diff] for sharing
#
# Returns the AE (absolute error pixel count) to stderr so CI / loops can
# fail when the screen drifts.
#
# Uses scripts/figma-fetch.sh (curl + jq) for the render; fall back to a
# pure python+curl path if jq isn't available.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

[[ $# -lt 3 ]] && die "usage: figma-diff.sh <fileKey> <nodeId> <reportDir>"

FILE_KEY="$1"
NODE_ID="$2"
DIR="$3"
mkdir -p "$DIR"

FIGMA_PNG="$DIR/figma.png"
AFTER_PNG="$DIR/after.png"
DIFF_PNG="$DIR/diff.png"
COMPARE_PNG="$DIR/compare.png"

[[ -f "$AFTER_PNG" ]] || die "missing $AFTER_PNG — capture the sim first: \
./scripts/screenshot-sim.sh \"$AFTER_PNG\""

# ── Download Figma render. Try the bash helper first; if it can't (jq), use
#    a pure-python fallback that re-uses the same token from .mcp.json.
HERE="$(cd "$(dirname "$0")" && pwd)"
if "$HERE/figma-fetch.sh" image "$FILE_KEY" "$NODE_ID" "$FIGMA_PNG" 2 \
        2>/dev/null; then
    :
else
    REPO_ROOT="$(cd "$HERE/.." && pwd)"
    FIGMA_REPO_ROOT="$REPO_ROOT" python3 - "$FILE_KEY" "$NODE_ID" "$FIGMA_PNG" <<'PY'
import json, os, sys, time, urllib.request, urllib.error
file_key, node_id, out = sys.argv[1:4]
token = os.environ.get('FIGMA_API_KEY')
if not token:
    here = os.environ['FIGMA_REPO_ROOT']
    cfg = json.load(open(os.path.join(here, '.mcp.json')))
    token = cfg['mcpServers']['figma']['env']['FIGMA_API_KEY']
def get(url):
    delay = 8
    for _ in range(6):
        try:
            req = urllib.request.Request(url, headers={'X-Figma-Token': token})
            return urllib.request.urlopen(req).read()
        except urllib.error.HTTPError as e:
            if e.code == 429:
                time.sleep(delay); delay = min(delay*2, 60); continue
            raise
    raise SystemExit('Figma API rate-limited')
meta = json.loads(get(
    f'https://api.figma.com/v1/images/{file_key}?ids={node_id}&format=png&scale=2'))
url = meta['images'][node_id]
open(out, 'wb').write(get(url))
print(f'wrote {out}', file=sys.stderr)
PY
fi

# Normalise both PNGs to the same canvas size (the larger of the two).
W=$(identify -format '%w' "$AFTER_PNG")
H=$(identify -format '%h' "$AFTER_PNG")

# Resize Figma render to match the sim's canvas exactly (force aspect so the
# pixel-diff is comparable position-for-position).
convert "$FIGMA_PNG" -resize "${W}x${H}!" "$DIR/.figma_resized.png"

# ── Pixel diff overlay (red where the two differ). AE goes to stderr.
COUNT=$(compare -metric AE -fuzz 5% \
    "$DIR/.figma_resized.png" "$AFTER_PNG" "$DIFF_PNG" 2>&1 || true)
echo "[figma-diff] AE (pixels different, 5% fuzz): $COUNT" >&2

# ── 3-up Figma | After | Diff with labels.
label() {
    convert "$1" -gravity north -background white -splice 0x24 \
        -fill '#333' -pointsize 14 -annotate +0+4 "$2" \
        -bordercolor '#bbb' -border 1x1 "$3"
}
label "$DIR/.figma_resized.png" "FIGMA"  "$DIR/.l_figma.png"
label "$AFTER_PNG"               "AFTER" "$DIR/.l_after.png"
label "$DIFF_PNG"                "DIFF"  "$DIR/.l_diff.png"
convert +append "$DIR/.l_figma.png" "$DIR/.l_after.png" "$DIR/.l_diff.png" \
    "$COMPARE_PNG"
rm -f "$DIR/.figma_resized.png" "$DIR/.l_figma.png" "$DIR/.l_after.png" \
      "$DIR/.l_diff.png"

echo "[figma-diff] wrote $DIFF_PNG"
echo "[figma-diff] wrote $COMPARE_PNG"
