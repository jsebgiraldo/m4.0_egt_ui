---
name: wireframe-flow
description: >
  Build a clean, navigable wireframe + flow diagram of the app's screens as a
  self-contained interactive HTML — screenshots as nodes, orthogonal wires that
  never cross a screen or cover a label, plus an automatic UX-issue audit
  (dead-ends, unreachable screens, missing exits). Use when the user asks for a
  wireframe, a screen map, a flow diagram, "how the screens connect", to
  document the current UI flow, or to review the navigation for UX problems.
---

# Wireframe & Flow diagram

Produce an **aesthetic, interactive** flow map of the EGT app: every screen as a
clickable image-node, connected by wires that follow the real navigation, with a
built-in UX audit. Output is a single `wireframe.html` (no CDN, works offline)
next to `screens/` (full-res) and `thumbs/` (labelled) PNG folders.

The engine is `wireframe_engine.py` (bundled in this skill). It encodes the
aesthetic rules so the diagram stays clean no matter how the flow grows.

## The aesthetic rules (why diagrams come out clean)

1. **Layered layout** — nodes live in columns (flow stages, `col`) and rows
   (`row`). Keep one logical stage per column, left→right in flow order.
   **Each column is vertically centered** around the global mid-line: columns
   have very different node counts (a 1-node menu column vs. the 7-row
   treatment column), and without centering every column top-aligns and the
   bottom half of the sparse ones is wasted empty space. Centering balances
   the canvas into a horizontal band and shortens the ring wires.
2. **Wires only travel in node-free channels**, so a wire never crosses a
   screen body:
   - adjacent-column edge → vertical run in the **gutter** between the columns;
   - forward edge skipping columns → up into the **top ring** (above all nodes),
     across, down into the target;
   - backward edge / loop → down into the **bottom ring**, across, up.
3. **Parallel segments get distinct lanes** (`LANE_STEP`) so wires never overlap.
4. **Labels sit on the clear segment with a solid plate**, drawn last (on top) —
   never covered by a wire or a node.
5. **UX audit**: flags dead-ends (no outgoing nav, unless in `TERMINALS`),
   unreachable screens (no path from the boot entry), and screens with no
   reachable exit. Shown in a side panel + a ⚠ badge on the node.

If a diagram still looks busy: widen `COL_STRIDE`/`ROW_STRIDE`, spread crowded
nodes onto more rows, or move a long edge's endpoints so it routes through a
shorter ring span. Never hand-place wires — fix the layout and regenerate.

## Workflow

1. **Capture every screen** from the simulator (serial — one X11 window at a
   time). Build the sim first (`cmake --build build-x86 -j4`), then for each
   screen state:
   ```bash
   pkill -x egt-app; sleep 1
   env EGT_START_SCREEN=<name> [EGT_MOCK_TREATMENT=<state>] ./build-x86/egt-app >/tmp/x 2>&1 &
   sleep 4
   ./scripts/screenshot-sim.sh docs/10-reports/wireframe/screens/<NN-name>.png
   pkill -x egt-app
   ```
   The full `EGT_START_SCREEN` list is in `src/app.cpp` (grep
   `std::string(start) ==`); treatment sub-states in
   `src/treatment/treatment_controller.cpp` (grep `EGT_MOCK_TREATMENT` /
   `s == "`). Name files `NN-<screen>` so they sort in flow order. For wizard
   steps not directly bootable (patient age/zip), navigate with `xdotool
   mousemove --window <wid> X Y click 1` (find the 800×480 window via
   `xdotool search --name '^EGT$'` + `getwindowgeometry --shell`).

2. **Make labelled thumbnails + a montage**:
   ```bash
   cd docs/10-reports/wireframe/screens
   for f in *.png; do convert "$f" -resize 360x216 -bordercolor '#ccc' -border 1 \
     -gravity South -background '#222' -fill white -pointsize 13 -splice 0x20 \
     -annotate +0+3 "${f%.png}" "../thumbs/$f"; done
   montage ../thumbs/*.png -tile 5x -geometry +6+6 -background white ../wireframe-all.png
   ```

3. **Build the flow model**: extract the real navigation edges from `app.cpp`
   (the `show_*` lambdas + their `on_*` callbacks) and treatment transitions.
   Edit `NODES` (id,label,file,col,row,section) and `EDGES`
   (from,to,label,kind: flow|branch|alert) in `wireframe_engine.py`. **Model the
   navigation that actually exists** — including back/close/loop edges — so the
   UX audit flags only genuine issues, not modelling gaps.

4. **Generate + verify**:
   ```bash
   cp .claude/skills/wireframe-flow/wireframe_engine.py docs/10-reports/wireframe/gen-wireframe.py
   cd docs/10-reports/wireframe && python3 gen-wireframe.py
   # headless sanity-check the layout (chromium snap remaps /tmp — write inside the repo):
   chromium --headless=new --no-sandbox --disable-gpu --hide-scrollbars \
     --window-size=2400,1800 --screenshot=check.png --virtual-time-budget=5000 \
     "file://$PWD/wireframe.html"
   ```
   Read `check.png`. Confirm: no wire over a screen, no covered label, the UX
   panel lists only real issues. Iterate the model/spacing, not the wires.
   Delete `check.png` before committing.

5. **Document + package**: write/refresh `README.md` (mermaid flow + per-section
   thumbnail tables) and link `wireframe.html`. To share, zip
   `wireframe.html + screens/ + thumbs/` together (relative paths — they must
   travel as a set).

## Conventions for this repo

- Output lives in `docs/10-reports/wireframe/`.
- `.gitignore` blocks `*.png`; the wireframe PNGs are allow-listed via
  `!docs/10-reports/wireframe/**/*.png` — keep that exception.
- All committed docs are in **English**.
- Reproduce any screen with `EGT_START_SCREEN=<name>` (treatment:
  `EGT_START_SCREEN=treatment EGT_MOCK_TREATMENT=<state>`).
