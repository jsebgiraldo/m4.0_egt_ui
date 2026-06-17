# Figma Parity — Fix Progress (on Fable)

**Last updated:** 2026-06-10, afternoon.
**Branch:** `develop`. Fonts (Gothic A1) **deferred** by owner decision — no
text width/weight diff was touched.

## Done and committed (ALL screens — full sweep)

| Commit | Screens |
|---|---|
| `af46af7` | demo-info, wifi-not-found, wifi-connecting, password |
| `9f3e861` | login, wifi-init (spinner), wifi-override-intro |
| `c4edf5c` | wifi-unavailable, wifi-override-info (popup), settings, wifi-settings |
| `1739735` | patient-gender (cards/buttons white, no border) |
| `5f788c2` | treatment ×6 (borderless buttons, shadows, dots, header) + error/warning ×5 (gradient, body word-wrap, buttons) |

Each batch: clean **host + ARM cross** build, recaptured, verified crash-free.
All fixes are geometry/color/shape/shadow — portable to EGT 1.10 (no
`draw(Pattern,RectF)`, cached SvgImage, screen-local colors, no shared tokens
touched).

## Remaining — NO mechanical fixes left. Only the DOUBTS.

All non-font, non-doubt screen fixes are applied and committed. The only thing
left is the design decisions (below) — once you resolve them they are a few-line
fix each.

## Token consumption (Fable)

| Batch | Agents | Tokens | Cap hit? |
|---|---|---|---|
| Probe (text-only) | 2 | 138,856 | no |
| Batch 2 (text-only) | 3 | 115,814 | no |
| Batch 3 (text-only) | 3 | 173,554 | no |
| Batch 4 (reads images) | 3 | 127,932 | **YES — exhausted** |

Lesson: **text-only** fixes (passing the report instead of the images) cost
~40-58k/agent and don't exhaust the cap. The **image-reading compare** is ~3×
more expensive and is what trips the limit — reuse existing reports and only
re-compare what changed (the 2 files above).

## Open doubts (your call — see DOUBTS.md)

D1 Gothic A1 font (deferred), D2 settings Account section, D3 treatment green
background, D4 countdown mm:ss, D5 patient technician icon (kept removed), D6
Back button pill, D7 override-intro bottom row (kept).
