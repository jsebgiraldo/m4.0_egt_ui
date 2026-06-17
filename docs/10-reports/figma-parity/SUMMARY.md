# Figma Parity Sweep — Executive Summary

**Scope:** all 26 app screens compared pixel-by-pixel against Figma file
`PBh3UuMmYwdFzalAyPTMBw`, then fixed to match (non-font diffs).
**Status:** complete. All fixes committed on `develop` — see `PROGRESS.md` for
the commit list, `DOUBTS.md` for the open product decisions.

## How it was done

Multi-agent workflow: one agent pulls the Figma render + exact spec
(colors/gradients/shadows/fonts) per screen; another captures the screen from
the simulator; another compares pixel-by-pixel (visual + ImageMagick + spec) and
writes a report. Those references and reports live in this directory (`figma/`,
`sim/`, `reports/`). A second pass of agents (on Fable, in small text-only
batches) applied the fixes; each batch was verified with a clean host + ARM-cross
build and a recapture. See `PROGRESS.md` for the token-cost breakdown.

## Main finding

**Gothic A1 is not installed** → all text falls back to DejaVu Sans and renders
~15-20% wider/heavier on ALL screens. This is the #1 root cause of most "diffs".
**Deferred by owner decision — see `DOUBTS.md` D1.** All other (non-font,
non-doubt) diffs have been fixed.

## Result

- **All 26 screens** had their non-font, non-doubt diffs applied and committed
  (banner colors, shadows, dividers, icon geometry, gradients, spinner banding,
  per-cycle dots, MM:SS countdown, popup alignment, borderless cards, …).
- Every change is portable to EGT 1.10 (no `draw(Pattern,RectF)`, cached
  SvgImage, screen-local colors, shared tokens untouched) and verified
  crash-free on host + ARM.
- What remains is **not** mechanical fixes — only the design decisions in
  `DOUBTS.md` (D1 font, D2 settings Account, D3 treatment green bg, D6 Back
  pill; D5/D7 assumed kept). D4 (countdown format) is resolved.

## Cross-cutting patterns that were addressed

1. **Banner orange** `#FFA500` → Figma `#FF9E1B` — set screen-local on
   wifi-unavailable, wifi-not-found, wifi-override-intro (shared token untouched).
2. **Stacked shadows** — replaced the dark/banded layer approximation with the
   `add_soft_shadow` Gaussian-falloff helper (wifi-not-found, override-intro,
   treatment buttons, HOME Start).
3. **Spinner barber-pole** — wifi-init and wifi-connecting shared the ring
   drawing code; the opaque-arc fix corrected both.
4. **SvgImage lifetime** — every raw slice now keeps the SvgImage alive in a
   static cache, hardening the EGT 1.10 heap-corruption class of crash.

## Still open (owner decisions)

See `DOUBTS.md`. The big one is **D1 (Gothic A1 font)** — until it's resolved at
the Linux/Yocto level, text parity is unreachable and the remaining visual gap on
every screen is just the fallback font.
