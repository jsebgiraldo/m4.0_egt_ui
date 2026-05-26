# M4.0 UI Improvement Report

Progress report for the Suntek M4.0 device user-interface alignment to the Figma v5 design (file [`Jason-M4.0-v5`](https://www.figma.com/design/PBh3UuMmYwdFzalAyPTMBw/Jason-M4.0-v5)). Each entry pairs a Figma target with the live simulator output and shows the gap closing.

The process behind each entry is documented in [figma-vs-sim-workflow.md](../04-ide-setup/figma-vs-sim-workflow.md): pull the design via Figma MCP, screenshot the simulator, diff, apply the change, screenshot again, compose a side-by-side image. Every dimension goes through `dt::SCALE = 1.852` (Figma canvas 432×261 pt → device panel 800×480 px) and colours stay on tokens (`dt::kGreen`, `dt::kTextPrimary`, ...) - no raw hex in the code.

## Screens completed

### Wi-Fi Connecting (`WIFI_INIT` - Figma node `2065:980`)

The boot spinner shown while the device looks for and joins a network. Brought into alignment with Figma v5: text changed to "Connecting to Wifi", logo enlarged and recentred in the ring, font bumped to match the scaled Gothic A1 spec.

![WIFI_INIT Before / Target / After](m4-18-wifi-init-figma-match/comparison.png)

*AFTER matches the Figma target within roughly ±4 px on key elements (logo, label, spinner geometry). The only visible delta is the spinner's gradient rotation, which animates in the live UI.*

---

### Wi-Fi Connected (`WIFI_CONNECTED` - Figma node `2065:1068`, Group 266)

The success card shown once the device joins the network. Re-laid out per Figma v5: the checkmark sits beside the title on the same row (instead of the previous stacked layout), the Lice Clinics logo grows to fill the top-left corner, body copy and the Continue button are positioned per the new spec.

![WIFI_CONNECTED Before / Target / After](m4-18-wifi-connected-figma-match/comparison.png)

*AFTER matches the Figma target within roughly ±4 px on all main elements. Every position, size, and font value comes from the Figma node multiplied by `dt::SCALE = 1.852`; colours stayed on existing tokens.*

---

### Home (`HOME` - Figma node `81:1507`, "S7-begin T")

The main menu of the device - three actions: a large blue gradient Start button to begin a treatment, plus Demo Mode and Setting cards at the bottom. Re-laid out per Figma v5: Lice Clinics logo at exact top-centre, Start button sized and positioned per spec, the two bottom cards shrunk and repositioned to Figma's `(20, 365)` and `(496, 365)`. Both card icons (person and gear) now load from real Figma PNG exports - including the gray circle backgrounds - instead of being drawn from inline SVG strings. Cards now show the Figma drop shadow via the shared `ShadowedCard` widget.

A follow-up pass tightened two remaining gaps: the Start button gradient was diagonal with ad-hoc cyan/blue colours, but Figma's export samples as a purely vertical gradient from `rgb(48,154,196)` at the top to `rgb(48,98,196)` at the bottom (R=48, B=196 constant, only G shifts). The gradient endpoint was switched to (x, y+h) and the colours moved to four new tokens (`kStartTop`, `kStartBottom`, plus pressed variants) in `palette.h`. The two bottom-card labels were left-aligned within their label box, which made the shorter "Setting" sit visibly off-centre next to the wider "Demo Mode"; Figma's spec is `textAlignHorizontal=CENTER`, so the alignment is now `center_horizontal | center_vertical`.

![HOME Before / Target / After](home-figma-match/comparison.png)

*AFTER matches the Figma target on layout, icon style, card sizing, drop shadow, gradient direction and colours, and label centring. Same `Gothic A1 Bold` font as the rest of the design, same `dt::kTextPrimary` for label colour.*

---

### Demo Info (`DEMO_INFO` - Figma node `84:608`, "Demonstration Mode") - M4-24

The screen shown after the user picks "Demo Mode" on Home. Three things were missing per ticket M4-24: the small Exit-demo button that sits under the DEMO MODE badge (top-right), and the left-chevron + circle icon on Back and the right-chevron + circle icon on Continue at the bottom. All three button glyphs were pulled directly from Figma as PNGs (`bt leave`, `bt EXIT`, `bt continue` component renders at scale=2) and placed via a small `make_image_button` helper that wraps an `ImageLabel` in a clickable `Frame`. Vertical positions of the green progress bar, divider, "TRAINING ONLY" heading, body text, and warning text were nudged 8-16 px down to match the Figma frame-local coordinates.

![DEMO_INFO Before / Target / After](m4-24-demo-info-figma-match/comparison.png)

*AFTER matches the Figma target on icons, button layout, and major positions. The button widths are a few pixels narrower than the target due to PNG aspect-fit (the PNGs include a small shadow padding), but the icon and text content are pixel-equivalent.*

---

<!--
TEMPLATE FOR THE NEXT ENTRY - copy, fill in, and slot above this comment.

### <Screen name> (`<SCREEN_ID>` - Figma node `<node:id>`)

One or two sentences describing what this screen is for and what changed.

![<SCREEN_ID> Before / Target / After](<task>-figma-match/comparison.png)

*AFTER matches the Figma target within roughly ±N px on key elements. <Optional note on any residual delta>.*

---
-->

## Pending screens

To be filled in as iterations complete.

## Notes for the reader

- Three states are shown per screen: **Before** (the previous on-device rendering), **Target (Figma)** (the design reference), **After** (the corrected on-device rendering).
- All comparisons are captured from the host simulator at the device resolution (800×480) so what you see is pixel-equivalent to the live panel.
- Where the design introduces an animation (a rotating spinner, a transitioning state), only the static AFTER frame is shown; the animation itself is preserved in code.
