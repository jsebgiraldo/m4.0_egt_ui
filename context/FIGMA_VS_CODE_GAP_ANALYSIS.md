# Figma vs Code — Gap Analysis

> Figma file: "Jason M4.0" (`fgxw4dmXVGV6Wvd7pAJ7Fb`)
> Codebase: `/home/sebas/workspace/suntek/m4/m4-egt-app/`
> Display: 800×480 (Figma uses 432×261 at ~1.85× scale)

---

## 1. Screen-by-Screen Comparison

### Legend
- ✅ Implemented (matches Figma)
- 🟡 Partially implemented (exists but needs rework)
- ❌ Not implemented (missing from code)

---

### A. Startup & Navigation Flow

| Figma Screen | Code File | Status | Gap Description |
|---|---|---|---|
| **HOME** — Logo + "Begin Treatment" + "Demo Mode" + "Setting" | `screen_mode_select.cpp` | 🟡 | Code has 3 stacked buttons ("Training Only", "Start Treatment", "Back"). Figma has large centered "Begin Treatment", small "Demo Mode" bottom-left, "Setting" gear bottom-right. Layout and wording differ completely. |
| **WiFi Connecting** — Spinner on startup | `screen_start.cpp` | 🟡 | Code has circular spinner (custom Cairo drawing). Works functionally but Figma shows a simpler screen. WiFi connection is implicit in Figma (no separate screen — just happens before HOME). |
| **WiFi Settings** — Network list + password | `screen_wifi_settings.cpp` + `screen_password_prompt.cpp` | 🟡 | Code has ListBox + password keyboard. Functionally close but UI polish differs from Figma. |
| **LOGIN** — Technician card grid (2×3) with avatars | `screen_login.cpp` | 🟡 | Code uses vertical ListBox with text-only items (Alice, Bob, Charlie, Dana). Figma shows 2×3 grid of photo cards with names underneath (Denzel Washington, Leonardo DiCaprio, etc.). Completely different layout. |
| **QWERTY Keyboard** — Password entry | `screen_password_prompt.cpp` | ✅ | Best match in the codebase. Design tokens, proper layout. Minor polish differences only. |

### B. Patient Information Flow (ALL MISSING)

| Figma Screen | Code File | Status | Gap Description |
|---|---|---|---|
| **PATIENT INFO — Gender** (Male / Female selection) | — | ❌ | Two large buttons: Male / Female + Back / Continue. Not in code at all. |
| **PATIENT INFO — Age** (number entry) | — | ❌ | Numeric input for age + Back / Continue. Not in code. |
| **PATIENT INFO — ZIP Code** (number entry) | — | ❌ | ZIP code entry + Back / Continue. Not in code. |
| **PATIENT INFO — Confirmation** | — | ❌ | Reviews entered data + Confirm / Back. Not in code. |

### C. Treatment Flow

| Figma Screen | Code File | Status | Gap Description |
|---|---|---|---|
| **DEMO MODE INFO** — "TRAINING ONLY" notice | — | ❌ | Tells user demo uses "lower temperature and lower fan speed". Not in code. |
| **WARMING UP** — Linear progress bar + "70%" | — | ❌ | Large percentage text + horizontal green progress bar. Completely missing. Code has circular spinner in `screen_start` but that's for WiFi, not warming. |
| **POSITION TIP** — "Position the Applicator Tip" + 5s countdown | — | ❌ | Large "0:05" countdown + instruction text + "Pause Treatment" / "End Treatment" buttons. Missing. |
| **TREATMENT ACTIVE** — Countdown timer + cumulative time | `screen_training.cpp` | 🟡 | Code has a single timer screen with Start/Pause/Cancel. Figma shows: large countdown number (e.g. "25"), segmented progress bar (6 segments), "Cumulative treatment time" counter at bottom, "End Treatment" / "Pause Treatment" buttons. Very different UI. |
| **NEARLY FINISHED** — Last 5 seconds countdown | — | ❌ | Shows "Treatment nearly finished" + countdown "5" + same End/Pause buttons. Missing in code (code just changes timer color at 30s and 10s). |
| **TREATMENT PAUSED** — Cumulative time + tip + Resume | — | ❌ | Shows "Treatment Paused" title, cumulative time, tip message ("Keep pauses short..."), "Resume Treatment" (filled) + "End Treatment" buttons. In code, pause just toggles a button label. |
| **RE-WARMING** — After resume, re-warm before next cycle | — | ❌ | Same as warming screen but after a pause/resume. Missing. |
| **RE-POSITION TIP** — After cycle completes | — | ❌ | "Reposition the Applicator Tip" between treatment cycles. Missing. |
| **END CONFIRMATION** — "Are you sure?" dialog | — | ❌ | Modal with "Are you sure you want to end treatment?" + "End Treatment" (filled) / "Cancel" buttons. Missing. |
| **TREATMENT COMPLETED** — Summary + "Back to Home" | — | ❌ | Shows cumulative treatment time (e.g. "00:30"), "Treatment Completed" with checkmark, "Back to Home" button. Missing. |
| **TREATMENT ENDED** (early end) — Summary | — | ❌ | Similar to completed but for early termination. Missing. |

### D. Error/Alert System (ALL MISSING)

| Error Type | Example Title | Example Message | Severity Color |
|---|---|---|---|
| **Informational** | "Change the air filter" | "Air filter replacement recommended — 1000 hours of use reached." | Blue banner |
| **Informational** | "Operating Conditions out of range" | "Only operating the device above the recommended temperature." | Blue banner |
| **Warning** | "Temperature out of range" | "Prevent further use of the machine to avoid ineffective treatments." | Yellow/orange banner |
| **Warning** | "Airflow obstruction detected" | "Please change the air filter before further use." | Yellow/orange banner |
| **Critical Fault** | "Operating temperature out of safe range" | "Device has been shut down to prevent hazard. Contact support before restarting. Error Code: E010" | Red banner |

### E. Visual Components (ALL MISSING)

| Component | Description | Status |
|---|---|---|
| **Header bar** | Lice Clinics logo (top-left) + screen title + mode/user indicators. Consistent across all screens. | ❌ |
| **DEMO MODE badge** | Cyan "DEMO MODE" text visible on all demo treatment screens | ❌ |
| **Leave button** (house icon) | Small button (60×24) to exit treatment/demo back to HOME | ❌ |
| **Segmented progress bar** | 6 rectangular segments (green = completed, gray = remaining) across top of treatment screens | ❌ |
| **Cumulative time footer** | Line separator + "00:05" + "Cumulative treatment time" label at bottom | ❌ |
| **Standard button components** | `bt new` (outlined) and `bt new over` (filled cyan) button templates | ❌ |

---

## 2. Architecture Gaps

### Current Architecture
```
app.cpp (lambdas orchestrate flow)
  └─ ScreenManager (remove_all → add → show)
       ├── screen_start        → WiFi spinner
       ├── screen_wifi_settings → network list
       ├── screen_login         → technician list
       ├── screen_mode_select   → training/treatment choice
       ├── screen_training      → single timer
       ├── screen_password_prompt → keyboard
       └── screen_welcome       → unused
```

### Issues
1. **No state machine** — Treatment flow needs: `WARMING → POSITION → ACTIVE → (NEARLY_FINISHED) → POSITION → ... → COMPLETED`. Currently it's one timer screen.
2. **No shared components** — Each screen rebuilds everything from scratch. No shared header, no standard buttons, no reusable progress bars.
3. **No Demo Mode concept** — Code has "training" (300s) vs "treatment" (600s) but no visual Demo Mode badge, no lower-temp/fan flag, no leave-to-home shortcut.
4. **No patient data model** — Gender, Age, ZIP code form is entirely missing.
5. **No error/alert system** — No way to show error overlays, no severity levels, no error codes.
6. **No transition animations** — ScreenManager does hard swap. Figma implies smooth flow.
7. **Screen navigation is tightly coupled** — Callbacks are lambdas defined in `app.cpp`. Adding 10+ new screens will make this unwieldy.

---

## 3. Proposed Architecture

```
app.cpp
  └─ AppController (state machine)
       ├── NavigationManager (stack-based, transitions)
       │
       ├── Shared Components:
       │     ├── HeaderBar (logo, title, demo badge, leave button)
       │     ├── ButtonStyles (outlined, filled, icon variants)
       │     ├── SegmentedProgressBar (6 segments)
       │     ├── CumulativeTimeFooter
       │     ├── ErrorOverlay (info/warning/critical)
       │     └── NumericKeypad / QWERTYKeyboard (already exists)
       │
       ├── Screens:
       │     ├── HomeScreen (Begin Treatment, Demo Mode, Setting)
       │     ├── LoginScreen (card grid with avatars)
       │     ├── PatientInfoScreen (gender → age → ZIP → confirm)
       │     ├── WiFiSettingsScreen (existing, improve)
       │     ├── DemoInfoScreen ("Training Only" notice)
       │     │
       │     └── TreatmentFlow/ (sub-state machine)
       │           ├── WarmingScreen (linear progress bar)
       │           ├── PositionTipScreen (5s countdown)
       │           ├── TreatmentActiveScreen (countdown + cumulative)
       │           ├── NearlyFinishedScreen (or state in Active)
       │           ├── TreatmentPausedScreen (resume/end)
       │           ├── EndConfirmationDialog (modal overlay)
       │           ├── TreatmentCompletedScreen (summary)
       │           └── TreatmentEndedScreen (early end summary)
       │
       └── Models:
             ├── PatientInfo { gender, age, zip_code }
             ├── TreatmentSession { mode, cumulative_time, cycles, technician }
             └── TechnicianProfile { name, avatar_path, password }
```

---

## 4. Implementation Priority

### Phase 1 — Core Screens & Architecture (Foundation)
1. Shared components: HeaderBar, ButtonStyles, ScreenBase class
2. HomeScreen (replace screen_mode_select)
3. LoginScreen (card grid with avatars)
4. Navigation refactor (stack-based NavigationManager)

### Phase 2 — Treatment Flow (Core Product)
5. WarmingScreen (linear progress bar)
6. PositionTipScreen (5s countdown)
7. TreatmentActiveScreen (countdown + cumulative time + segmented progress)
8. NearlyFinishedScreen
9. TreatmentPausedScreen
10. EndConfirmationDialog
11. TreatmentCompletedScreen / TreatmentEndedScreen
12. Treatment state machine (cycle management)

### Phase 3 — Patient & Data Entry
13. PatientInfoScreen (gender/age/ZIP wizard)
14. DemoInfoScreen
15. Demo Mode badge + leave button

### Phase 4 — Error System & Polish
16. ErrorOverlay component (3 severity levels)
17. Error code definitions + display logic
18. WiFi screen improvements
19. Screen transition animations
20. Asset generation (technician avatars, icons)

---

## 5. Open Questions

See section in main analysis — several design decisions need clarification before implementation.
