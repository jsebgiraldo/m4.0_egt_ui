# Figma Parity — Open Questions (decisions I need from you)

> Produced by the comparison sweep of the 26 screens against Figma
> (file `PBh3UuMmYwdFzalAyPTMBw`). These are the items I **did not touch**
> because they need a decision from you, not a mechanical fix. Ordered by
> impact. Resolve these and the rest are straightforward code fixes.

---

## 🔴 D1 — Gothic A1 is NOT installed (root cause #1, affects ALL screens)

**What happens:** the app requests `Font("Gothic A1", …)` everywhere, but Gothic
A1 is not installed — not on the simulator host, not in the repo, not in any
Yocto recipe. `fc-match "Gothic A1"` falls back to **DejaVu Sans**. Result:
**every label on every screen** renders ~15-20% wider and heavier than Figma.

Most of the "differences" the agents reported (text too wide, shifted titles,
lines clipped at the card edge) are **a single symptom of this**, not
independent layout bugs.

**Decision I need:**
- (a) Does the target Yocto image **already** ship Gothic A1? If so, the
  mismatch is only the simulator environment and the target renders fine —
  nothing to fix in code, just install the font in the sim to validate.
- (b) If the target **also** lacks it: we must **bundle the Gothic A1 TTFs**
  (Regular/Medium/Bold — it's a Google Font, OFL license, free) into assets and
  register them with fontconfig at startup.
- (c) If we don't want to ship the font: shrink every size ~10% and accept the
  fallback face (it will never be pixel-identical to Figma).

**I recommend (b)** — it's the only way to reach "identical to Figma". Until
this is resolved it's not worth chasing per-screen text-width diffs because they
all change once the correct font is in.

---

## 🟠 D2 — Settings: "Account / Technician Login" section vs Figma's "About this device"

**What happens:** Figma Settings (node 2073:1996) shows an **"About this device"**
section at the bottom with a one-line summary (firmware, serial, IP). Our sim
instead shows an **"Account"** section with the **"Technician Login"** button,
and pushes "About this device" below the fold (invisible without scrolling).

**But** — you deliberately added the Technician Login button earlier in this
project. So this is **probably an approved product addition, NOT a bug**.

**Decision:** does Technician Login stay in Settings (accepted deviation from
Figma) or do we remove it to match the design's "About this device"? If it
stays, there is still a minor bug: the card gets **clipped in half** by the
scroll viewport edge (`screen_settings.cpp:716`) — I can fix that.

---

## 🟠 D3 — Treatment "active": green-gradient background or white?

**What happens:** Figma node 143:862 (treatment active) shows the background with
a **green gradient** that fades at the top and bottom (`#5BC500 → #FFF →
#5BC500`). Our sim draws it **flat white**. The `GreenGlow` widget already exists
in the code and draws exactly this effect — it's just disabled for this state.

**The doubt:** frame 143:862 shows a 3-second countdown and 26/30 dots filled —
hallmarks of a **near-end** moment of the cycle, not the steady active state.
It's unclear whether the green gradient applies to the whole active state or
only near the end of the cycle.

**Decision:** does the green gradient apply to the whole "active" state or only
"nearly finished"? Affects `treatment_controller.cpp:825` (pass
`green_mode=true`).

---

## ✅ D4 — Countdown format `mm:ss` vs bare seconds — RESOLVED

Resolved 2026-06-10 (commit `4f57cdd`): all treatment countdowns now use
zero-padded **`MM:SS`** in both normal and demo mode (active `00:25`, nearly
`00:05`, position `00:05`, cycle-end `00:00`).

---

## 🟡 D5 — Patient gender: technician icon in the header (you removed it)

**What happens:** Figma (2009:1262) shows a small 28×28 profile icon to the
right of the "Please Enter Client Information" title. You **deliberately removed
it** earlier (comment in `screen_patient_info.cpp` "removed per requirement
update").

**Decision:** does it stay removed (accepted deviation) or do I re-add it to
match Figma 1:1? I'm assuming it stays removed unless you say otherwise.

---

## 🟡 D6 — "Back" button: pill with shadow vs Figma's bare chevron

**What happens:** on several screens Figma draws Back as a chevron in a gray
circle + a "Back" label **directly on the white background**, with no pill or
shadow. Our sim uses a white rounded card with a drop shadow (the shared
`demo-info-btn-back` PNG in `components.cpp:685`).

**Decision:** is the pill the app-wide standard (it appears on other polished
screens) or do we switch to Figma's bare chevron? Since it's a **shared
component**, changing it affects every screen at once — that's why I left it.

---

## 🟢 D7 — wifi-override-intro: bottom Back/Retry/Setting row outside the Figma frame

**What happens:** the wifi-override-intro screen (134:1421) in Figma does **not**
include the bottom Back/Retry WiFi/Setting button row. We added it (mirrored from
wifi-unavailable) so the user can exit. It's an **intentional** UX addition.

**Decision:** does the bottom row stay (intentional deviation) or do we remove it
to match the exact Figma frame? I'm assuming it stays.

---

## Dynamic-content notes (NOT defects — already filtered out)

- **Brightness slider**: the thumb position reflects the device's real
  brightness, not a layout bug.
- **Countdown / cumulative-time / override-day values**: come from runtime state
  or `EGT_MOCK_TREATMENT`; the content differs from the Figma placeholder by
  design. (Curiosity: Figma itself is inconsistent — the background card says "6"
  days and the popup says "7".)
- **About this device** (firmware/serial/IP): read from `/etc`/sysfs at runtime;
  the text will never match the Figma placeholder.
