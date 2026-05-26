#!/usr/bin/env bash
# Print a Markdown "Memory footprint" block for docs/10-reports/ui-improvement-report.md.
# Run it from the project root:  ./scripts/report-memory.sh
# Pipe to xclip or paste the output into the report's footer section.

set -euo pipefail
# Force C locale so awk uses dots (not commas) as the decimal separator,
# regardless of the developer's LANG setting.
export LC_ALL=C

cd "$(dirname "$0")/.."

BIN=build-x86/egt-app
[[ -f "$BIN" ]] || { echo "error: $BIN not found - build first (./scripts/run-simulator.sh --build)" >&2; exit 1; }

# `size` (binutils) prints text/data/bss; the file size on disk is bigger
# (includes the ELF headers + relocations + debug, depending on build flags).
read text data bss _ _ _ < <(size "$BIN" | awk 'NR==2')
file_size=$(stat -c %s "$BIN")
seg_total=$((text + data + bss))

human() {
    # bytes -> human-readable, decimal MB / KB
    local b=$1
    if (( b >= 1048576 )); then
        awk -v b="$b" 'BEGIN { printf "%.2f MB", b/1048576 }'
    elif (( b >= 1024 )); then
        awk -v b="$b" 'BEGIN { printf "%.1f KB", b/1024 }'
    else
        echo "${b} B"
    fi
}

# Asset totals
png_total=$(find assets -name '*.png' -printf '%s\n' 2>/dev/null | awk '{s+=$1} END {print s+0}')
svg_total=$(find assets -name '*.svg' -printf '%s\n' 2>/dev/null | awk '{s+=$1} END {print s+0}')
embedded_total=$(grep -oE '_len = [0-9]+' src/generated/embedded_assets.h 2>/dev/null | awk -F'= ' '{s+=$2} END {print s+0}')
runtime_total=$((png_total + svg_total - embedded_total))

today=$(date +%F)

cat <<EOF
## Memory footprint

Snapshot $today. Refresh with \`scripts/report-memory.sh\`.

- **Binary (egt-app):** $(human "$file_size") on disk; loaded sections $(human "$seg_total") (text $(human "$text") + data $(human "$data") + bss $(human "$bss")).
- **Assets:** $(human $((png_total + svg_total))) total in \`assets/\`. $(human "$embedded_total") is embedded into the binary via \`src/generated/embedded_assets.h\`; the rest ($(human "$runtime_total")) loads from disk at runtime.
- **Fonts:** 0 bytes in repo. \`Gothic A1\` is requested by all screens and must be present in the target rootfs (loaded via fontconfig at runtime).

### Top asset files

| File | Size |
|---|---|
EOF
find assets -type f \( -name '*.png' -o -name '*.svg' \) -printf '%s\t%p\n' \
    | sort -rn | head -10 \
    | awk -F'\t' '
        function human(b) {
            if (b >= 1048576) return sprintf("%.2f MB", b/1048576)
            if (b >= 1024)    return sprintf("%.1f KB", b/1024)
            return sprintf("%d B", b)
        }
        {
            sub("assets/figma/images/", "", $2)
            sub("assets/", "", $2)
            printf "| %s | %s |\n", $2, human($1)
        }
    '
