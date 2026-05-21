#!/usr/bin/env bash
# Direct wrapper around the Figma REST API for the Figma-vs-sim workflow.
#
# Bypasses the figma-developer-mcp stdio server (which keeps dropping mid-session)
# and uses the personal access token already configured in ~/.claude.json. This
# is the same token the MCP uses internally - we just skip the wrapper.
#
# Commands:
#   figma-fetch.sh node    <fileKey> <nodeId> [outJson]
#       Fetch layout/spec JSON for a node. Prints to stdout or writes to outJson.
#
#   figma-fetch.sh image   <fileKey> <nodeId> <outPng> [scale]
#       Download the node rendered as PNG. Default scale=2 for sharpness.
#
#   figma-fetch.sh index   <fileKey> [outJson]
#       Walk the entire file and build a flat searchable index of every
#       node with {id, name, type, text, parent_id, path}. Use as a local
#       cache so you do not need to hit the network for "what's in v5?".
#       Default output: docs/08-references/figma-v5-snapshot.json
#
#   figma-fetch.sh search  <substring> [indexJson]
#       Grep the cached index for nodes whose name or text contains the
#       given substring (case-insensitive). One match per line.
#
# Auth:
#   The token is read in this order:
#     1. $FIGMA_API_KEY env var
#     2. ~/.claude.json (any FIGMA_API_KEY value found via recursive descent)
#   Override the path with $CLAUDE_JSON.
#
# Requires: curl, jq.

set -euo pipefail

CLAUDE_JSON="${CLAUDE_JSON:-$HOME/.claude.json}"

die() { echo "error: $*" >&2; exit 1; }

require() { command -v "$1" >/dev/null || die "missing dependency: $1"; }
require curl
require jq

load_token() {
    if [[ -n "${FIGMA_API_KEY:-}" ]]; then
        printf '%s' "$FIGMA_API_KEY"
        return 0
    fi
    if [[ -f "$CLAUDE_JSON" ]]; then
        local tok
        tok=$(jq -r '.. | objects | select(.FIGMA_API_KEY?) | .FIGMA_API_KEY' \
              "$CLAUDE_JSON" 2>/dev/null | head -1)
        if [[ -n "$tok" && "$tok" != "null" ]]; then
            printf '%s' "$tok"
            return 0
        fi
    fi
    die "no Figma token found (set FIGMA_API_KEY or configure it in ~/.claude.json)"
}

TOKEN="$(load_token)"

api() {
    # api <path-with-query>  -> JSON to stdout
    local path="$1"
    curl -sS --fail-with-body \
         -H "X-Figma-Token: $TOKEN" \
         "https://api.figma.com$path"
}

cmd_node() {
    local file_key="${1:-}" node_id="${2:-}" out="${3:-}"
    [[ -z "$file_key" || -z "$node_id" ]] && die "usage: node <fileKey> <nodeId> [outJson]"
    local json
    # ids= takes the colon form directly (e.g. 2065:980)
    json=$(api "/v1/files/$file_key/nodes?ids=$node_id")
    if [[ -n "$out" ]]; then
        mkdir -p "$(dirname "$out")"
        printf '%s' "$json" > "$out"
        echo "[figma-fetch] wrote $out"
    else
        printf '%s' "$json"
    fi
}

cmd_image() {
    local file_key="${1:-}" node_id="${2:-}" out="${3:-}" scale="${4:-2}"
    [[ -z "$file_key" || -z "$node_id" || -z "$out" ]] && \
        die "usage: image <fileKey> <nodeId> <outPng> [scale=2]"
    mkdir -p "$(dirname "$out")"
    local resp url
    resp=$(api "/v1/images/$file_key?ids=$node_id&format=png&scale=$scale")
    url=$(printf '%s' "$resp" | jq -r --arg id "$node_id" '.images[$id] // empty')
    [[ -z "$url" || "$url" == "null" ]] && die "Figma did not return a render URL for $node_id (response: $resp)"
    curl -sSL --fail-with-body -o "$out" "$url"
    echo "[figma-fetch] saved $out ($(file -b "$out"))"
}

# Walk the response of /v1/files/<key> and emit one flat row per node.
# Each row: id, name, type, text (if TEXT), parent_id, path (parent name chain).
emit_index() {
    jq -c '
        def walk($parent_id; $path):
            . as $n |
            [{
                id:        ($n.id            // ""),
                name:      ($n.name          // ""),
                type:      ($n.type          // ""),
                text:      ($n.characters    // null),
                parent_id: $parent_id,
                path:      $path
            }] +
            ([$n.children // empty | .[] | walk($n.id; ($path + "/" + ($n.name // "?")))] | add // []);
        .document | walk(""; "")
        | .[]
    '
}

cmd_index() {
    local file_key="${1:-}" out="${2:-docs/08-references/figma-v5-snapshot.json}"
    [[ -z "$file_key" ]] && die "usage: index <fileKey> [outJson]"
    mkdir -p "$(dirname "$out")"
    echo "[figma-fetch] fetching full file structure (this can take 5-15 s)..." >&2
    local resp
    resp=$(api "/v1/files/$file_key")
    # Wrap rows in a JSON array
    {
        printf '['
        printf '%s' "$resp" | emit_index | paste -sd ','
        printf ']'
    } | jq '.' > "$out"
    local count
    count=$(jq 'length' "$out")
    echo "[figma-fetch] wrote $out  ($count nodes)"
}

cmd_search() {
    local needle="${1:-}" idx="${2:-docs/08-references/figma-v5-snapshot.json}"
    [[ -z "$needle" ]] && die "usage: search <substring> [indexJson]"
    [[ -f "$idx" ]] || die "index not found: $idx  (run 'figma-fetch.sh index <fileKey>' first)"
    jq -r --arg q "$needle" '
        ($q | ascii_downcase) as $needle |
        .[] | select(
            ((.name // "") | ascii_downcase | contains($needle)) or
            ((.text // "") | ascii_downcase | contains($needle))
        )
        | "\(.id)\t[\(.type)]\t\(.name)\t\(.text // "")"
    ' "$idx"
}

case "${1:-}" in
    node)   shift; cmd_node   "$@" ;;
    image)  shift; cmd_image  "$@" ;;
    index)  shift; cmd_index  "$@" ;;
    search) shift; cmd_search "$@" ;;
    -h|--help|"") sed -n '2,30p' "$0" ;;
    *) die "unknown command: $1  (use node, image, index, or search)" ;;
esac
