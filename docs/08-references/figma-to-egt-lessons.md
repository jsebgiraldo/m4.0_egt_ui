# Figma -> EGT lessons learned

Practical patterns and traps discovered while bringing screens into pixel parity with Figma v5. Read this before starting the next screen iteration. Update it after each iteration.

Companion docs:

- [egt-widget-reference.md](egt-widget-reference.md) — deep reference on EGT widget semantics (the "why").
- [../04-ide-setup/figma-vs-sim-workflow.md](../04-ide-setup/figma-vs-sim-workflow.md) — the iteration loop itself (the "how").

---

## What worked (do this every time)

### 0. Install Figma's font on the host FIRST

**Read this before doing anything else on a new screen.** Figma's text rendering uses the font the designer picked (this project: Gothic A1). The host simulator's `fontconfig` falls back to whatever is installed when the requested font is missing — usually NotoSans on Ubuntu / WSL. Two different fonts means two different glyph widths means "Continue" ends in two different places means every downstream positioning iteration is chasing a font mismatch, not a layout mismatch.

Check before iterating:

```bash
fc-match "Gothic A1:weight=700"
# If this returns NotoSans-Bold (or anything that is not GothicA1-Bold), STOP.
# Install Gothic A1 first.
```

Install Gothic A1 (one-time, per host):

```bash
mkdir -p ~/.local/share/fonts/gothic-a1
cd ~/.local/share/fonts/gothic-a1
for w in Regular Medium SemiBold Bold; do
    curl -sSfLO "https://github.com/google/fonts/raw/main/ofl/gothica1/GothicA1-${w}.ttf"
done
fc-cache -f ~/.local/share/fonts/gothic-a1
fc-match "Gothic A1:weight=700"      # should now return GothicA1-Bold.ttf
```

Then in code, **always pass the family name to `Font(...)` explicitly**:

```cpp
label->font(Font("Gothic A1", 26, Font::Weight::bold));
// not Font(26, Font::Weight::bold) — that uses the theme default.
```

For any other font the designer introduces, repeat: install it on the host, request it by name in the code.

**On the target device** (SAMA Yocto image) the available fonts are different (Lato, NotoSans, NotoSansCJKsc, NotoColorEmoji per `docs/04-ide-setup/simulator.md` and Jira M4-17). Matching the simulator to Figma is the priority for the report; if a screen must look identical on the panel, add the font family to the Yocto image recipe (M4-19 is the analogous task for splash.bmp).

### 1. Figma REST API > Figma MCP for everything

The MCP server (`figma-developer-mcp`) keeps disconnecting mid-session. Use `scripts/figma-fetch.sh` directly:

```bash
./scripts/figma-fetch.sh search   "Wi-Fi Connected"     # find node id in the cached index
./scripts/figma-fetch.sh node     <fileKey> <nodeId>    # pull layout/spec JSON
./scripts/figma-fetch.sh image    <fileKey> <nodeId> <out.png> 4   # 4x render
```

The index at `docs/08-references/figma-v5-snapshot.json` is searchable offline and contains every node's name + text + parent path. Refresh only when the designer pushes a redesign.

### 2. Fetch icons, do not draw them

For every non-text element that has a distinct visual (chevrons, checkmarks, gear icons, error banners, dividers): export the PNG from Figma at 4x scale and place it as an `ImageLabel`. Drawing the icon with `Painter` polygons / strokes never matched Figma's weight and proportions on the first try and ate iterations.

Workflow per icon:

```bash
./scripts/figma-fetch.sh image <fileKey> <nodeId> assets/figma/images/<icon-name>.png 4
```

Then in code, **always** pre-scale the `Image` so its natural size matches the target rect — this is what disarms the autoresize trap (next section):

```cpp
constexpr int target_w = 22;        // Figma 12 px * dt::SCALE
constexpr int target_h = 39;        // Figma 21 px * dt::SCALE
const float hscale = static_cast<float>(target_w) / png_natural_w;
const float vscale = static_cast<float>(target_h) / png_natural_h;
auto img = Image("file:assets/figma/images/<icon-name>.png", hscale, vscale);
auto widget = std::make_shared<ImageLabel>(img);
widget->autoresize(false);
widget->image_align(AlignFlag::center);
widget->box(Rect(x, y, target_w, target_h));
```

### 3. `autoresize(false)` on every widget you place at a fixed Figma rect

This is the single most important lesson. Both `Button` and `ImageLabel` (anything with a non-trivial `min_size_hint()`) will silently grow past the rect you set during a later layout pass. Call `autoresize(false)` **immediately after construction**, before any other property change.

```cpp
auto btn = std::make_shared<Button>("Continue", Rect(0, 0, 204, 61));
btn->autoresize(false);          // FIRST — locks the rect
btn->font(Font(26, Font::Weight::bold));   // these no longer grow the box
btn->text_align(AlignFlag::left | AlignFlag::center_vertical);
// ...
```

The same applies to `ImageLabel`.

### 4. Skip the helper functions when order matters

`ui::create_outlined_button(text, rect, on_click)` calls `font(dt::fontButton())` internally **before** you can flip `autoresize(false)`. The button is already grown by the time you get the pointer back. For any button that must sit at a Figma-driven rect, build it inline instead of using the helper, so you control the order of operations.

If we end up doing this a lot, extend the helper signature to accept an `autoresize_lock = true` flag and have it set `autoresize(false)` before `font()`.

### 5. Wrap button + icon in a sibling `Frame`

When two widgets need to share an absolute coordinate region (button outline + icon overlay), wrap them in a `Frame` and add both as children of the wrapper. Both then use **wrap-local coords** (origin at the wrapper's top-left), so positioning is consistent and a layout pass on one cannot push the other.

```cpp
auto btn_wrap = std::make_shared<Frame>(Rect(302, 355, 204, 61));
btn_wrap->fill_flags({});                   // transparent
container->add(btn_wrap);

auto btn = std::make_shared<Button>("Continue", Rect(0, 0, 204, 61));
btn->autoresize(false); /* ... */;
btn_wrap->add(btn);

auto chevron = std::make_shared<ImageLabel>(img);
chevron->autoresize(false);
chevron->image_align(AlignFlag::center);
chevron->box(Rect(160, 11, 22, 39));         // wrap-local coords
btn_wrap->add(chevron);
```

### 6. Use `box(Rect)` over `move() + resize()`

`move()` then `resize()` is two layout-triggering ops. `box(Rect)` is one. With `autoresize(true)` (the default) either pattern can trip the grow-back behaviour; with `autoresize(false)` they are functionally equivalent. Prefer `box(Rect)` for clarity.

### 7. `image_align(AlignFlag::center)` only — no `expand`

`expand` makes the image scale to fill the widget's content area while preserving ratio, which then anchors the visible image to the top-left of the box (not centred). Result: the icon visually drifts upward and leftward. Use plain `center`.

### 8. For exact Figma-driven button content: Button = card, Label = text, ImageLabel = icon

`egt::Button`'s built-in text rendering uses the widget's font metrics, theme padding, and `text_align()` — none of which know anything about Figma's `absoluteBoundingBox`. Even with `text_align(AlignFlag::left | AlignFlag::center_vertical)` the text starts at the border offset and ends wherever the font's intrinsic glyph width takes it. With a 26 pt Bold font in a 204 px-wide button the text ended ~30 px short of where Figma's bbox put it, so the chevron to the right looked spread out instead of "balanced".

The pattern that gets you Figma-1:1 spacing every time:

1. Use the `Button` as **just the outlined card + click handler** — pass an empty string and no font.
2. Add a separate `Label` for the visible text, positioned at the exact Figma button-local rect.
3. Add `ImageLabel`s for any icons, also at exact Figma button-local rects.

```cpp
// Outlined card + click handling (no text)
auto btn = std::make_shared<Button>("", Rect(0, 0, 204, 61));
btn->autoresize(false);
btn->color(Palette::ColorId::button_bg, dt::kWhite);
btn->color(Palette::ColorId::border,    dt::kGrayLight);
btn->border(2);
btn->border_radius(dt::RADIUS_MD);
btn->on_click([cb = std::move(on_continue)](Event&) { cb(); });
btn_wrap->add(btn);

// "Continue" label at the Figma TEXT node's rect (button-local)
auto lbl = std::make_shared<Label>("Continue", Rect(7, 17, 154, 33));
lbl->autoresize(false);
lbl->font(Font(26, Font::Weight::bold));
lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
btn_wrap->add(lbl);

// Chevron at the Figma icon node's rect (button-local)
btn_wrap->add(chevron);   // configured above
```

Every position is now Figma-derived, not font-metric-derived. If the design changes, you change the rects; nothing in the rendering pipeline reaches for "default" behaviour.

### 9. The 3-up `comparison.png` is the deliverable

Before / Target / After horizontally, with 1 px gray dividers (per `figma-vs-sim-workflow.md` step 9). Only this image goes into the client report and the Jira comment — never the raw intermediates.

---

## What did not work (do not repeat)

### Manual icon drawing via `Painter`

Drew the chevron as two stroked line segments inside a custom `Widget`. Looked too thin, the stroke weight never matched Figma's filled polygon, and positioning was off because of the autoresize trap on the surrounding widget. **Always export the icon from Figma.**

### Unicode glyph fallback for icons (`❯`, `›`, `▶`)

Tried using `"\xe2\x9d\xaf"` (U+276F HEAVY RIGHT-POINTING ANGLE QUOTATION MARK) inside the button text. The runtime font (NotoSans on the target, system font on the host) does not have the glyph -> renders as a tofu box. Only a handful of arrows are guaranteed across the installed fonts, and none of them match the visual weight of the Figma chevron. **Do not rely on Unicode arrows.** Use the PNG.

### Sizing an icon from the source polygons instead of the rendered shape

When the icon you exported is a Figma `BOOLEAN_OPERATION` (for example a chevron built as `polygon A minus polygon B`), Figma's spec lists three nodes for it: the two source polygons (e.g. 13 x 21 each) and the boolean result itself (e.g. 9 x 13). The PNG you export is the boolean RESULT — so its real visible content is the 9 x 13 shape, not the 13 x 21 polygons.

If you size the widget from the polygon dimensions you get an icon ~30 % bigger than Figma's. **Always pull the bounding box of the `absoluteBoundingBox` field on the node you actually exported**, not the source shapes:

```bash
jq -r '.. | objects | select(.id?=="<node-id>") | "\(.id) \(.absoluteBoundingBox)"' /tmp/spec.json
```

For node 2065:1063 the answer was `width: 8.68, height: 12.99` -> widget 17 x 24, not 22 x 39.

### `+30 px` magic offset to "fix" the position

Symptom: chevron rendered ~30 px above where the rect spec said. First instinct was to nudge `y` by +30. That hid the symptom but the box was still growing to the image's natural size (52 px) — the icon just happened to look centred after the hack. Root cause was autoresize, fix is `autoresize(false)`. **If a widget is N px off, find out why before reaching for a magic offset.**

### Using `ui::create_outlined_button` then calling `autoresize(false)`

The helper has already called `font()` internally by the time the pointer is returned, so the button has already grown past the requested rect. `autoresize(false)` afterwards prevents *future* growth but does not shrink the box back. **Order matters. Build inline when order matters.**

### `padding(28)` on the button to push the text left

Padding contributes to `min_size_hint()`. With `autoresize(true)`, adding padding silently grew the button to fit text + padding. The right way to push the text left is `text_align(AlignFlag::left | AlignFlag::center_vertical)`.

### `move()` then `resize()` inside a screen-sized container

Pattern: place an ImageLabel at `move(Point(462, 372))` then `resize(Size(22, 39))`. Worked for the logo widget elsewhere in the codebase, did not work here. Cause was the autoresize trap, not the calls themselves — but the codepath obscured what was happening. **Use `box(Rect)` so the diff between intent and behaviour is in one line of code.**

---

## Pre-flight checklist for the next screen

Before writing any code:

- [ ] Run `fc-match "<font name>:weight=700"` for every font the design uses. If anything returns a different family, install the missing font (see section 0 at the top of this file) before doing anything else.
- [ ] Run `./scripts/figma-fetch.sh search "<screen text>"` to confirm the node exists in v5.
- [ ] Run `./scripts/figma-fetch.sh image <fileKey> <node> <task>-figma-match/figma-target.png 2` to grab the reference render.
- [ ] Run `./scripts/figma-fetch.sh node <fileKey> <node> /tmp/spec.json` for the layout JSON.
- [ ] Identify which sub-nodes are icons (`type == REGULAR_POLYGON`, `BOOLEAN_OPERATION`, `IMAGE-SVG`, `VECTOR`) and queue them for PNG export.
- [ ] Confirm the simulator can park on this screen (existing `--wifi-static` / `--wifi-init` flags, or add a new `EGT_MOCK_WIFI=<name>` mode).
- [ ] Capture `before.png` first so the iteration has a baseline.

When writing the code:

- [ ] Every `Label` / `Button` text request: `Font("<family>", <scaled-pt>, weight)` with the family name from Figma. Never `Font(<scaled-pt>, weight)` alone — that uses the theme default and silently uses the wrong font.
- [ ] Every `Button` / `ImageLabel` placed at a Figma rect: `autoresize(false)` immediately after construction.
- [ ] Every icon: `Image(uri, hscale, vscale)` so the natural size matches the target, plus `image_align(AlignFlag::center)`.
- [ ] Group icon + button (or any two widgets sharing a region) in a wrapper `Frame`.
- [ ] No raw `egt::Color(r, g, b)` — use a `dt::` token. Add to `palette.h` only if Figma genuinely introduces a new value.
- [ ] No Unicode arrows / chevrons / icon glyphs inside text labels.

Before declaring done:

- [ ] AFTER matches TARGET within ±4 px on every key element in the 3-up comparison.
- [ ] No `+N` magic offset in the position math anywhere.
- [ ] One-line `// Figma v5 node <id>: <element> <raw>x<raw> at (<raw>,<raw>) -> <scaled>x<scaled> at (<x>,<y>)` comment near each placement so the next reader can trace any pixel back to Figma.
- [ ] New section appended to `docs/10-reports/ui-improvement-report.md` with the comparison image.
- [ ] This file updated with anything new learned.

---

## Open EGT limitations (do not waste iterations trying to fix)

- **Drop shadows on Buttons / cards** render slightly differently between the host (cairo + X11) and the target (cairo + KMS/DRM). Aim for "close enough" rather than pixel parity.
- **Font kerning / hinting** on the host uses fontconfig's installed fonts; the target ships only NotoSans / NotoSansCJKsc / NotoColorEmoji. Slight glyph-width differences are expected. Do not chase these unless they push a layout off by more than ~4 px.
- **Anti-aliasing of stroked arcs** (the WiFi spinner ring) shows visible "stripes" at 800x480 because the ring is drawn as ~120 short arc segments. Matching Figma's perfect conic gradient would need a custom `cairo_pattern_create_conic` integration. Out of scope for now.
- **Unicode glyph fallback** is unreliable (see above). Stick to ASCII text + Figma-exported icons.

If a limitation comes up that is not on this list, add it here so we do not relitigate it on a future screen.
