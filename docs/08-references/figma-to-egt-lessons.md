# Figma -> EGT lessons learned

Practical patterns and traps discovered while bringing screens into pixel parity with Figma v5. Read this before starting the next screen iteration. Update it after each iteration.

Companion docs:

- [egt-widget-reference.md](egt-widget-reference.md) - deep reference on EGT widget semantics (the "why").
- [../04-ide-setup/figma-vs-sim-workflow.md](../04-ide-setup/figma-vs-sim-workflow.md) - the iteration loop itself (the "how").

---

## What worked (do this every time)

## Figma canvas vs device panel aspect ratio

The Figma canvas and the device panel do not share an aspect ratio:

|  | Width | Height | Aspect |
|---|---|---|---|
| Figma canvas | 432 pt | **261.47** pt | **1.6522** |
| Device panel | 800 px | 480 px | **1.6667** |

The X axis maps exactly: `800 / 432 = 1.8519`. The Y axis does not: `480 / 261.47 = 1.8358`.

Per-axis scales (defined in `src/ui/design_tokens.h`):

```cpp
dt::SCALE_X = 1.8519   // 800 / 432
dt::SCALE_Y = 1.8358   // 480 / 261.47
dt::SCALE   = SCALE_X  // canonical uniform scale used by every screen
```

**Project convention: use `dt::SCALE` (= 1.852) uniformly for both axes.** This keeps X exact and accepts a ~1 % vertical drift. Practically, the bottom 3-4 px of Figma canvas (y > ~258 in design coords) gets clipped at the device, but no design element reaches that low today so it has been invisible in every iteration.

If a future layout has an element that genuinely must hit the device's bottom edge, pass `dt::SCALE_Y` explicitly for the Y math while keeping `dt::SCALE` for the X math. **Do not retrofit `SCALE_Y` across the existing screens** -- their layout numbers were chosen against `SCALE_X`, and switching axes mid-codebase would shift everything up by a few pixels.

The comparison images had a different but related problem: `convert -resize 500x -gravity north` preserved aspect ratio, so the Figma render came out 3 px taller than the sim screenshots and looked "raised." Fixed by forcing `-resize "500x300!"` on every panel before composition (see `figma-vs-sim-workflow.md` step 9).

---

## The Figma-1:1 rule (F1:1)

This is the canonical construction rule for any widget that must match Figma exactly. **Every screen iteration MUST follow it before any visual tweaking.** If something looks off, the answer is "we broke one of these invariants", not "let's nudge a pixel".

| # | Invariant | Why it matters |
|---|---|---|
| 1 | **Geometry from Figma `absoluteBoundingBox` × `dt::SCALE`** | Anything else is guesswork. |
| 2 | **Zero moat on every widget**: `w->border(0); w->padding(0); w->margin(0);` | EGT's default theme adds a 2 px border to every widget. With a 2 px moat your visible content is offset 2 px from the box you set. |
| 3 | **`autoresize(false)` on widgets where the BOX is the visual** (`Button`, `ImageLabel`, custom shapes). **Leave `autoresize(true)` on `Label` (text) widgets** | Buttons grow to fit text, ImageLabels grow to image natural size - both bad, lock them. Labels need to be able to grow if our cairo + the Figma font render glyphs slightly wider than Figma's engine - locking the box clips the last letter. Position the Label at Figma's top-left and let the box grow rightward / downward as needed; the *anchor* matches Figma, the *trailing edge* tolerates rendering drift. |
| 4 | **Explicit font family in every `Font(...)`**: `Font("<family>", size * dt::SCALE, weight)` | `Font(size, weight)` uses the theme default family. fontconfig silently falls back to NotoSans when the Figma family is missing. Different glyph widths = different layout. |
| 5 | **Colours from `dt::` tokens** | If Figma introduces a new hex, add to `palette.h` first, never inline. |
| 6 | **Icons via PNG**: export from Figma at 4× and use `ImageLabel` with `Image(uri, hscale, vscale)` pre-scaled. Never draw shapes with Painter primitives. | Painter strokes will not match Figma's render at the first try. PNG + pre-scaled load disarms the auto-resize trap and gives pixel parity. |
| 7 | **Effects via custom Painter widget**: drop shadows, gradients, conic patterns. **EGT clips Painter to the widget's `box()` - the widget must be larger than the visible artwork.** For a drop shadow with N px spread, the widget grows by N on every side and the visible shape is drawn centred inside. | EGT has no native blur or conic gradient. Approximate with layered rounded rects (shadows) or arc segments (gradients). Wrap that drawing in a tiny widget so it composes cleanly. The clipping rule cost an hour of debugging the first time - the shadow was visible only at the rounded corners (where painting happened to fall inside the box) and looked like four dotted marks instead of a halo. |
| 8 | **One wrapper `Frame` per Figma `GROUP`** at the group's bbox, with `fill_flags({})` (transparent) and the same zero-moat treatment. Children of the group use group-local coords. | Keeps coordinate systems consistent and isolates layout effects so one widget cannot shove another. |
| 9 | **Button = `ShadowedCard` or empty `Button` + `Label` overlay + `ImageLabel` overlay** | `Button`'s built-in text uses font metrics, not Figma coordinates. Separate widgets at Figma rects always wins. |

Pre-flight check (run before writing any code for a screen):

```bash
fc-match "<font name>:weight=700"                  # font installed?
./scripts/figma-fetch.sh search "<screen text>"    # node id confirmed?
./scripts/figma-fetch.sh node <key> <id> /tmp/spec.json   # spec on disk
```

Extract the spec card per child node (one line each: id, type, bbox, characters verbatim, text alignment H+V, font family + weight + size, any effects):

```bash
jq -r '.nodes."<rootId>".document | .. | objects
  | select(.type? // empty | test("RECTANGLE|TEXT|VECTOR|BOOLEAN_OPERATION|GROUP|FRAME"))
  | "\(.id)\t[\(.type)]\t\(.name)\tbb=(\(.absoluteBoundingBox.x|floor),\(.absoluteBoundingBox.y|floor) \(.absoluteBoundingBox.width|floor)x\(.absoluteBoundingBox.height|floor))\tchars=\"\(.characters // "")\"\talign=\(.style.textAlignHorizontal // "-")/\(.style.textAlignVertical // "-")\tfont=\(.style.fontFamily // "-") \(.style.fontWeight // "-") \(.style.fontSize // "-")pt\teffects=\([.effects[]? | .type] | join(\",\") // \"-\")"' \
  /tmp/spec.json
```

**Critical for TEXT nodes**: pull the literal `characters` string with whitespace preserved (Figma often stores `"Continue "` with a trailing space to balance layout against an adjacent icon) and BOTH alignment fields (`textAlignHorizontal` and `textAlignVertical`). Map those into the EGT `Label` directly. Note: Figma's TEXT bbox is tightly fit to the line-height of the actual glyphs (e.g. 18 px for 14 pt); when you scale that bbox up by `dt::SCALE` the glyphs no longer fill it. With `textAlignVertical=TOP` in code, the scaled glyphs sit at the top of the scaled bbox and look raised above centre. The fix is in the Button recipe below - span the label across the FULL parent height and use `center_vertical`, not the scaled bbox height with TOP.

**Critical for GROUP / FRAME nodes**: pull the `effects` array. A `DROP_SHADOW` effect means the group needs a `ShadowedCard`-style widget; an `INNER_SHADOW` or `LAYER_BLUR` means custom drawing. Plain groups with no effects are just wrapper `Frame`s.

Then every widget in the code maps one-to-one to a row in that spec, with the F1:1 invariants applied.

---

## Button construction recipe

This is the proven, working pattern for any "outlined card with text + icon" button in the design (the Continue button on WIFI_CONNECTED is the canonical example). Follow it step by step on the next button.

### Step 1 - gather the spec from Figma

A Figma button is usually a `GROUP` containing four kinds of children:

| Figma child type | Maps to |
|---|---|
| `RECTANGLE` (the background) | dimensions of the card |
| `TEXT` (the label) | a `Label` widget |
| `VECTOR` / `BOOLEAN_OPERATION` / `IMAGE-SVG` (the icon) | a `ImageLabel` widget loading a PNG exported from Figma at 4× |
| (parent `GROUP`'s `effects`) | `ShadowedCard` if `DROP_SHADOW`, plain `Frame` otherwise |

Extract once with the jq one-liner above. Note the parent `GROUP`'s `effects` separately (jq filter for the root group id) - that's where the drop shadow spec lives.

### Step 2 - coordinate system

The parent `GROUP` defines the button-local coordinate system. All child Figma coordinates are RELATIVE to the GROUP origin once you compute `child.absoluteBoundingBox - group.absoluteBoundingBox`. Multiply by `dt::SCALE` to get our panel coordinates.

If the button has a drop shadow, the wrapper Frame in code is enlarged by `SHADOW_PAD` (12 px) on every side to give the shadow room to render. All child positions inside the wrap are then biased by `+SHADOW_PAD`.

### Step 3 - code template

```cpp
// Figma <group-id>: outlined button with drop shadow.
constexpr int   PAD          = ShadowedCard::SHADOW_PAD;
constexpr float card_radius  = 7.0f;                    // Figma cornerRadius * SCALE
const Rect card_rect(<x>, <y>, <w>, <h>);               // RECTANGLE child bbox * SCALE
const Rect wrap_rect(card_rect.x() - PAD, card_rect.y() - PAD,
                     card_rect.width()  + 2 * PAD,
                     card_rect.height() + 2 * PAD);

// Wrapper frame (transparent), enlarged by PAD on every side.
auto btn_wrap = make_shared<Frame>(wrap_rect);
btn_wrap->fill_flags({});
container->add(btn_wrap);

// White card + drop shadow + click handling, drawn in wrap-local coords.
auto btn = make_shared<ShadowedCard>(
    Rect(PAD, PAD, card_rect.width(), card_rect.height()),
    card_radius,
    std::move(on_click));
btn_wrap->add(btn);

// Text label: span the full button height, center both ways. Do NOT use
// the scaled TEXT bbox height directly with vertical=TOP - the scaled
// glyphs leave empty space at the bottom of the bbox and the text
// renders above the button centre.
auto lbl = make_shared<Label>("<characters-verbatim>",
    Rect(PAD + <text_x>, PAD, <text_w>, card_rect.height()));
lbl->border(0); lbl->padding(0); lbl->margin(0);
lbl->font(Font("<family-from-Figma>", <pt * SCALE>, Font::Weight::<weight>));
lbl->color(Palette::ColorId::label_text, dt::<colour-token>);
lbl->text_align(AlignFlag::center_horizontal | AlignFlag::center_vertical);
btn_wrap->add(lbl);

// Icon: PNG exported from Figma at 4×, sized to the visible bbox of the
// node you exported (NOT the source polygons). autoresize(false) and
// pre-scale Image at load.
constexpr int icon_w = <visible-bbox-w * SCALE>;
constexpr int icon_h = <visible-bbox-h * SCALE>;
constexpr int icon_src_w = <png-natural-w>;
constexpr int icon_src_h = <png-natural-h>;
const float hscale = static_cast<float>(icon_w) / icon_src_w;
const float vscale = static_cast<float>(icon_h) / icon_src_h;
auto icon_img = Image("file:assets/figma/images/<name>.png", hscale, vscale);
auto icon = make_shared<ImageLabel>(icon_img);
icon->autoresize(false);
icon->border(0); icon->padding(0); icon->margin(0);
icon->fill_flags({Theme::FillFlag::blend});
icon->image_align(AlignFlag::center);
icon->box(Rect(PAD + <icon_x>, PAD + <icon_y>, icon_w, icon_h));
btn_wrap->add(icon);
```

### Step 4 - debug border (optional)

When something looks off, temporarily uncomment a 1 px black `painter.stroke()` on the card path inside `ShadowedCard::draw()` to see the card bounds exactly. Remove before committing.

### Step 5 - verify

- Visible button outline (shadow halo) matches Figma's intensity (~10 % alpha cumulative).
- Text glyph centre lines up with button centre vertically.
- Text + icon spacing matches Figma - if they're spread too far apart, you trimmed the trailing whitespace in the `characters` string. Restore it.
- Icon is `~17 % too big`? You used the source-polygon dimensions instead of the visible `BOOLEAN_OPERATION` bbox. Re-pull from the right node.

The `ShadowedCard` widget itself currently lives inline at the top of `src/screens/screen_wifi_connected.cpp`. When the second screen needs it, move both `ShadowedCard` and `draw_rounded_path` into `src/ui/components.h` / `components.cpp` and include from there.

---

### 0. Install Figma's font on the host FIRST

**Read this before doing anything else on a new screen.** Figma's text rendering uses the font the designer picked (this project: Gothic A1). The host simulator's `fontconfig` falls back to whatever is installed when the requested font is missing - usually NotoSans on Ubuntu / WSL. Two different fonts means two different glyph widths means "Continue" ends in two different places means every downstream positioning iteration is chasing a font mismatch, not a layout mismatch.

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
// not Font(26, Font::Weight::bold) - that uses the theme default.
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

Then in code, **always** pre-scale the `Image` so its natural size matches the target rect - this is what disarms the autoresize trap (next section):

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
btn->autoresize(false);          // FIRST - locks the rect
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

### 7. `image_align(AlignFlag::center)` only - no `expand`

`expand` makes the image scale to fill the widget's content area while preserving ratio, which then anchors the visible image to the top-left of the box (not centred). Result: the icon visually drifts upward and leftward. Use plain `center`.

### 8. For exact Figma-driven button content: Button = card, Label = text, ImageLabel = icon

`egt::Button`'s built-in text rendering uses the widget's font metrics, theme padding, and `text_align()` - none of which know anything about Figma's `absoluteBoundingBox`. Even with `text_align(AlignFlag::left | AlignFlag::center_vertical)` the text starts at the border offset and ends wherever the font's intrinsic glyph width takes it. With a 26 pt Bold font in a 204 px-wide button the text ended ~30 px short of where Figma's bbox put it, so the chevron to the right looked spread out instead of "balanced".

The pattern that gets you Figma-1:1 spacing every time:

1. Use the `Button` as **just the outlined card + click handler** - pass an empty string and no font.
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

Before / Target / After horizontally, with 1 px gray dividers (per `figma-vs-sim-workflow.md` step 9). Only this image goes into the client report and the Jira comment - never the raw intermediates.

---

## Treatment flow screens (reusable findings)

Findings from the Treatment Active screen (Figma 66:524) that the other treatment
screens (Completed, Paused, Position, Nearly-done, End-confirm) should reuse, so each
one needs fewer iterations.

**Park on a screen with the hold mode.** The treatment screens auto-advance on timers.
`EGT_START_SCREEN=treatment-demo EGT_MOCK_TREATMENT=<screen>` boots straight onto one
screen with its countdown frozen. Screens: `warming | ready | position | reposition |
active | nearly | paused | end-confirm | completed | ended`. Use `treatment-demo` (not
`treatment`) so the DEMO MODE badge shows - the Figma treatment frames are the demo
variant. The freeze sets sane timings (process limit 2700 s, 25 s cycle, 00:05
cumulative) so demo mode's tiny limits don't drive the countdown negative.

**Treatment font tokens are unscaled - scale every font by 1.852 yourself.** `dt::FONT_*`
in design_tokens.h hold small raw values (FONT_BODY=18, FONT_HUGE=100) that are *not*
Figma px x SCALE. On treatment screens, set fonts from the Figma pt directly:

| Element | Figma | Code |
|---|---|---|
| Big countdown number | 64 pt **Regular** | `Font(116, normal)` - NOT `dt::fontHuge()` (it's bold + small) |
| Status line ("Treatment started") | 16 pt | `Font(28, normal)` |
| Cumulative time value | 24 pt | `Font(40, normal)` |
| "CUMULATIVE TREATMENT TIME" label | 10 pt | `Font(18, normal)` |
| Action button verb ("Pause"/"End") | 18 pt bold | `Font(33, bold)` |
| Action button qualifier ("Treatment") | 11 pt | `Font(20, normal)` |

**The shared treatment widgets now match Figma - reuse, don't rebuild:**

- `make_action_button()` (treatment_controller.cpp) renders the Figma "bt new" button:
  white card 131x54->244x100, gray text #646569, two-line verb+qualifier centred
  vertically, soft layered drop shadow (Figma `boxShadow 0 2 2 rgba(0,0,0,0.2)`).
  **Both Pause and End are this same white card** - End is NOT green. Pass
  `BTN_OUTLINED` style.
- `ui::create_segmented_progress()` is the Figma 202x5 bar: a near-continuous row of
  small 5px **squares** (square corners, ~2px gaps), filled green by width via
  `update_segmented_progress_fraction()`. The "32px segments" in the spec JSON are group
  bounding boxes, each holding 5 small squares - they are NOT 5 long blocks.
- `create_demo_mode_badge(..., Card)` is the Figma demo badge: 50%-opacity cyan "DEMO
  MODE" text (no card behind it) + the real `demo-info-btn-exit.png` exit button (the
  Figma `bt leave` 84:565). Shared layout constants: `STATUS_Y=240`, `DOTS_Y=285`,
  `BTN_W=244`, `BTN_H=100`, `BTN_BOTTOM_MARGIN=21`, `BTN_LEFT_X=39`, `BTN_RIGHT_X=537`.

**More treatment-screen findings (warming / position / paused / completed):**
- Big numbers are **thin**: `Font(116, Font::Weight::normal)` for countdowns,
  `Font(120, normal)` inside `PercentDisplay`. EGT has no Light (300) weight, so
  `normal` is the floor - the Figma render is a hair thinner and that gap is an
  accepted EGT limitation (same on the Active screen).
- Number format is per-screen: Active shows bare seconds ("25"), Position shows
  "M:SS" with a single minute digit ("0:05" - not `format_time`'s "00:05"),
  Paused/Completed show the cumulative "MM:SS".
- Position (100:772) has **no** cumulative-time header - call
  `make_treatment_container(state, false)`. Its status is two gray `Font(28)`
  lines ("Position the" / "Applicator Tip"); a single small `fontBody` line read
  brownish from subpixel rendering.
- Paused (67:773) tip: two centred cyan `Font(18,bold)` lines flanked by cyan
  chevron SVGs. When writing an SVG to /tmp then loading it with `SvgImage`,
  **scope the `ofstream` in braces** so it flushes before the read - otherwise
  the icon renders blank.
- Completed (75:320) is the "Back to Home" screen: cumulative header + a centred
  blue `ResultGlyph` check beside blue "Treatment Completed" + a cyan
  `make_back_home_button()`. The Figma blue is a cyan->blue gradient; EGT can't
  gradient-fill text, so use the midpoint `Color(48,129,196)` as a solid.
- Nearly-finished (67:578) has a **breathing green glow** (Figma note 67:723:
  "the green is to reflect the effect of flashing light"). A hard on/off rounded
  border looked cheap - use a `GreenGlow` custom widget instead: four edge
  gradients (green -> transparent, ~90 px inward) drawn BEHIND the content
  (same idea as the age-wheel `WheelBackdrop`, applied full-screen), with an
  `m_intensity` (0..1) scaling the alpha. Animate it with a 50 ms timer easing
  intensity along `0.12 + 0.88*(0.5 - 0.5*cos(phase))` (~2.6 s per breath) - a
  cosine ease reads as breathing; a square toggle reads as a cheap blink. Make
  the widget `readonly(true)` and keep it the first child so it never eats
  button taps. Gate `start()` on `!freeze` (the still holds a mid-bright glow);
  `EGT_FLASH_TEST=1` forces breathing while held so you can capture a GIF.
  Verify by sampling an edge pixel across frames - it should ramp smoothly, not
  jump between two values.

## Patient Info wizard screens (reusable findings)

Findings from the demo-mode Patient Info steps (Gender `2009:1262`, Age `2009:1173`,
ZIP `2009:1060`, Summary `2009:962`). These four share `make_patient_step()` and the
helpers below, so a fix on one lands on all four (and on the non-demo flow too).

**Reach each step in the simulator.** Only `EGT_START_SCREEN=patient-info-demo` exists
(it boots the Gender step). For Age/ZIP/Summary, click through with `xdotool` against the
`^EGT$` window: Female card `(503,250)`, Continue `(650,417)`; ZIP keys are at column x
`{215,311,407,503,598}` / row y `{213,302}`. Screen rebuilds on each tap but positions
stay fixed, so chained clicks work.

**Multi-stop vertical gradients - two ways, both verified:**
- Custom `Widget::draw`: `painter.draw(Pattern(Pattern::StepArray{{0,c0},{0.33f,c1},...},
  Point(x,y), Point(x,y+h)), RectF(x,y,w,h))`. Used for the age-wheel cylinder backdrop
  (`WheelBackdrop`). Figma's 0.9-alpha edge shading reads near-black stacked on our panel;
  drop it to ~0.15-0.27 alpha to match the soft gray the Figma frame actually renders.
- Button fill: `btn->color(Palette::ColorId::button_bg, Pattern(StepArray, p0, p1))` with
  the points in the button's absolute rect coords. Respects `border_radius`. Used for the
  inactive ZIP keys (gray `217->255` "number bt" gradient, no border).

**The shared patient widgets now match Figma - reuse, don't rebuild:**
- Back/Continue glyphs `kArrowBackSvg` / `kArrowFwdSvg` are **chevrons** (`<` / `>`), not
  shafted arrows. Figma "bt continue" / "bt EXIT" use a chevron-in-circle.
- `create_demo_mode_badge(..., Compact)` is the patient-step badge: 50%-opacity cyan
  "DEMO MODE" (Font 26) over the real `demo-info-btn-exit.png` (`bt leave`). Smaller than
  the treatment `Card` variant. Never draw the arrow.
- Age wheel: `WheelBackdrop` cylinder gradient + a "Years Old" caption (Bold `Font(18)`,
  `#646569`) to the right of the wheel.
- ZIP keypad: pressed digits use `create_green_button` (#5BC500, white text); unpressed
  use `create_gradient_key` (gray gradient, no border).
- Summary header: call `make_patient_step(2, ..., /*as_pills=*/true)` - all three steps
  render as outlined white pill tabs with a **full-width** green bar (not a partial
  under-tab indicator). The review block is **borderless** - two `rgba(0,0,0,0.1)` divider
  lines, no card. Back + GO sit together centre-bottom (both `156x61`, x=233 / x=424).

## What did not work (do not repeat)

### Manual icon drawing via `Painter`

Drew the chevron as two stroked line segments inside a custom `Widget`. Looked too thin, the stroke weight never matched Figma's filled polygon, and positioning was off because of the autoresize trap on the surrounding widget. **Always export the icon from Figma.**

### Unicode glyph fallback for icons (`❯`, `›`, `▶`)

Tried using `"\xe2\x9d\xaf"` (U+276F HEAVY RIGHT-POINTING ANGLE QUOTATION MARK) inside the button text. The runtime font (NotoSans on the target, system font on the host) does not have the glyph -> renders as a tofu box. Only a handful of arrows are guaranteed across the installed fonts, and none of them match the visual weight of the Figma chevron. **Do not rely on Unicode arrows.** Use the PNG.

### Sizing an icon from the source polygons instead of the rendered shape

When the icon you exported is a Figma `BOOLEAN_OPERATION` (for example a chevron built as `polygon A minus polygon B`), Figma's spec lists three nodes for it: the two source polygons (e.g. 13 x 21 each) and the boolean result itself (e.g. 9 x 13). The PNG you export is the boolean RESULT - so its real visible content is the 9 x 13 shape, not the 13 x 21 polygons.

If you size the widget from the polygon dimensions you get an icon ~30 % bigger than Figma's. **Always pull the bounding box of the `absoluteBoundingBox` field on the node you actually exported**, not the source shapes:

```bash
jq -r '.. | objects | select(.id?=="<node-id>") | "\(.id) \(.absoluteBoundingBox)"' /tmp/spec.json
```

For node 2065:1063 the answer was `width: 8.68, height: 12.99` -> widget 17 x 24, not 22 x 39.

### `+30 px` magic offset to "fix" the position

Symptom: chevron rendered ~30 px above where the rect spec said. First instinct was to nudge `y` by +30. That hid the symptom but the box was still growing to the image's natural size (52 px) - the icon just happened to look centred after the hack. Root cause was autoresize, fix is `autoresize(false)`. **If a widget is N px off, find out why before reaching for a magic offset.**

### Using `ui::create_outlined_button` then calling `autoresize(false)`

The helper has already called `font()` internally by the time the pointer is returned, so the button has already grown past the requested rect. `autoresize(false)` afterwards prevents *future* growth but does not shrink the box back. **Order matters. Build inline when order matters.**

### `padding(28)` on the button to push the text left

Padding contributes to `min_size_hint()`. With `autoresize(true)`, adding padding silently grew the button to fit text + padding. The right way to push the text left is `text_align(AlignFlag::left | AlignFlag::center_vertical)`.

### `move()` then `resize()` inside a screen-sized container

Pattern: place an ImageLabel at `move(Point(462, 372))` then `resize(Size(22, 39))`. Worked for the logo widget elsewhere in the codebase, did not work here. Cause was the autoresize trap, not the calls themselves - but the codepath obscured what was happening. **Use `box(Rect)` so the diff between intent and behaviour is in one line of code.**

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

- [ ] Every `Label` / `Button` text request: `Font("<family>", <scaled-pt>, weight)` with the family name from Figma. Never `Font(<scaled-pt>, weight)` alone - that uses the theme default and silently uses the wrong font.
- [ ] Every `Button` / `ImageLabel` placed at a Figma rect: `autoresize(false)` immediately after construction.
- [ ] Every icon: `Image(uri, hscale, vscale)` so the natural size matches the target, plus `image_align(AlignFlag::center)`.
- [ ] Group icon + button (or any two widgets sharing a region) in a wrapper `Frame`.
- [ ] No raw `egt::Color(r, g, b)` - use a `dt::` token. Add to `palette.h` only if Figma genuinely introduces a new value.
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
