#!/usr/bin/env bash
# Compose a consistent BEFORE / TARGET / AFTER comparison.png for a UI report screen.
#
# Usage: scripts/compose-comparison.sh <report-dir>
#   <report-dir> holds the source frames. The script auto-detects:
#     before : before.png            (optional - omitted for brand-new screens)
#     target : *target*.png          (the Figma reference)
#     after  : after.png             (the corrected sim rendering)
#
# Every screen is normalized to 800x480, wrapped in a thin gray frame with a
# white inner gutter, and given an italic label in the white margin above it,
# so the label never sits on top of the screen pixels. Panels are joined with
# equal white margins. Output is written to <report-dir>/comparison.png.
set -euo pipefail

DIR="${1:?usage: compose-comparison.sh <report-dir>}"
DIR="${DIR%/}"
[ -d "$DIR" ] || { echo "no such dir: $DIR" >&2; exit 1; }

SCREEN_W=800
SCREEN_H=480
GUTTER=4                 # white padding between screen and frame
FRAME=2                  # gray frame thickness
FRAME_COLOR="#9aa0a6"
LABEL_H=58               # height of the label margin above each screen
LABEL_PT=30
LABEL_COLOR="#333333"
LABEL_FONT="DejaVu-Sans-Oblique"
MARGIN_X=22              # white margin left/right of each panel
MARGIN_TOP=14
MARGIN_BOTTOM=20

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Build one panel: normalized screen + frame + label margin above. $1=src $2=label $3=out
build_panel() {
  local src="$1" label="$2" out="$3"
  # Normalize to 800x480 on white (fit, then pad/center).
  convert "$src" -resize ${SCREEN_W}x${SCREEN_H} -background white -gravity center \
    -extent ${SCREEN_W}x${SCREEN_H} "$TMP/norm.png"
  # White gutter, then gray frame.
  convert "$TMP/norm.png" -bordercolor white -border ${GUTTER} \
    -bordercolor "${FRAME_COLOR}" -border ${FRAME} "$TMP/framed.png"
  local fw
  fw="$(identify -format '%w' "$TMP/framed.png")"
  # Label margin (white) with centered italic label.
  convert -size ${fw}x${LABEL_H} xc:white -gravity center \
    -font "${LABEL_FONT}" -pointsize ${LABEL_PT} -fill "${LABEL_COLOR}" \
    -annotate +0+0 "$label" "$TMP/label.png"
  # Stack label over framed screen.
  convert "$TMP/label.png" "$TMP/framed.png" -append "$out"
}

# Detect sources.
BEFORE=""
[ -f "$DIR/before.png" ] && BEFORE="$DIR/before.png"
AFTER="$DIR/after.png"
TARGET="$(ls "$DIR"/*target*.png 2>/dev/null | head -1 || true)"
[ -n "$TARGET" ] || { echo "no *target*.png in $DIR" >&2; exit 1; }
[ -f "$AFTER" ] || { echo "no after.png in $DIR" >&2; exit 1; }

PANELS=()
if [ -n "$BEFORE" ]; then
  build_panel "$BEFORE" "BEFORE" "$TMP/p_before.png"; PANELS+=("$TMP/p_before.png")
fi
build_panel "$TARGET" "TARGET (Figma)" "$TMP/p_target.png"; PANELS+=("$TMP/p_target.png")
build_panel "$AFTER" "AFTER" "$TMP/p_after.png"; PANELS+=("$TMP/p_after.png")

# Equal white margin around each panel, then join.
FRAMED=()
for p in "${PANELS[@]}"; do
  fp="${p%.png}_m.png"
  convert "$p" -bordercolor white -border ${MARGIN_X}x0 \
    -background white -gravity north -splice 0x${MARGIN_TOP} \
    -background white -gravity south -splice 0x${MARGIN_BOTTOM} "$fp"
  FRAMED+=("$fp")
done

convert "${FRAMED[@]}" +append -bordercolor white -border 4x0 "$DIR/comparison.png"
echo "wrote $DIR/comparison.png ($(identify -format '%wx%h' "$DIR/comparison.png"))"
