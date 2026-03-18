# M4.0 UI Design Map — Figma Reference

**Figma File:** [Jason M4.0](https://www.figma.com/design/fgxw4dmXVGV6Wvd7pAJ7Fb/Jason-M4.0)
**Frame:** `M4.0 UI design - V3.`
**Screen Size:** 432 × 261 px (maps to 800 × 480 physical display on SAMA5D27)

---

## Screen Flow Overview

```
┌──────────┐   ┌───────────┐   ┌──────────────┐   ┌─────────────┐
│  HOME    │──▶│  LOGIN    │──▶│ PATIENT INFO │──▶│  WARMING UP │
│ Group197 │   │ Group172  │   │ Group148/204 │   │ Grp47/179   │
└──────────┘   └───────────┘   └──────────────┘   └──────┬──────┘
                                                         │
                                     ┌───────────────────▼──────────────────┐
                                     │         POSITION TIP                 │
                                     │  Demo: Grp209→Grp48  Reg: Grp214→181│
                                     └───────────────────┬──────────────────┘
                                                         │
                                     ┌───────────────────▼──────────────────┐
                                     │       TREATMENT ACTIVE               │
                                     │  Demo: Grp150   Regular: Grp184      │
                                     └──┬────────────────┬──────────────┬───┘
                                        │                │              │
                                   [Pause]          [End Trt]     [Complete]
                                        │                │              │
                               ┌────────▼───┐   ┌───────▼────┐  ┌──────▼──────┐
                               │  PAUSED    │   │ END CONFIRM│  │ NEARLY DONE │
                               │ Grp160/-   │   │ Grp51/187  │  │ Grp151/185  │
                               └────┬───────┘   └────┬───────┘  └──────┬──────┘
                                    │                 │                 │
                               [Resume]          [Confirm]        [Auto 0]
                                    │                 │                 │
                            ┌───────▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐
                            │ RE-WARMING   │  │  ENDED      │  │ COMPLETED   │
                            │ Grp162/195   │  │ Grp178/194  │  │ Grp174/193  │
                            └──────────────┘  └─────────────┘  └─────────────┘
```

---

## 1. Startup & Navigation Screens

| Figma Group | Node ID | Screen Name | Description |
|-------------|---------|-------------|-------------|
| Group 197 | `81:1507` | **HOME** | Main menu: Begin Treatment, Demo Mode, Settings |
| Group 172 | `73:1664` | **USER LOGIN** | Technician selection list (names) |
| Group 171 | `72:1538` | **KEYBOARD INPUT** | QWERTY keyboard for text entry |
| Group 117 | `41:2267` | **KEYBOARD INPUT v2** | Second QWERTY keyboard variant |

## 2. Patient Info / Mode Selection

| Figma Group | Node ID | Screen Name | Description |
|-------------|---------|-------------|-------------|
| Group 203 | `84:608` | **DEMO MODE INFO** | Explains demo mode (lower temp/fan). Back, Continue, Training Only |
| Group 148 | `63:334` | **PATIENT INFO - GENDER** | Regular mode. Male/Female selection |
| Group 204 | `84:652` | **PATIENT INFO - AGE** | Demo mode. Age entry after gender selection |
| Group 231 | `115:929` | **PATIENT INFO FORM** | Full form: Gender, Age, ZIP Code, GO/Reset |

## 3. DEMO MODE Treatment Flow (left side of canvas)

| Figma Group | Node ID | Screen Name | Description |
|-------------|---------|-------------|-------------|
| Group 47 | `37:1626` | **DEMO - WARMING UP** | 70% progress, DEMO MODE badge |
| Group 209 | `100:772` | **DEMO - POSITION TIP 0:05** | Countdown 5s, End/Pause buttons |
| Group 48 | `37:1635` | **DEMO - POSITION TIP 0:01** | Countdown 1s, almost ready |
| Group 150 | `66:524` | **DEMO - TREATMENT ACTIVE** | Active countdown, End/Pause buttons |
| Group 151 | `67:578` | **DEMO - NEARLY FINISHED** | 00:25 cumulative, about to complete |
| Group 152 | `67:628` | **DEMO - TREATMENT COMPLETED** | 00:30 cumulative, counter at 0 |
| Group 160 | `67:773` | **DEMO - PAUSED** | 00:27 cumulative, Resume Treatment |
| Group 162 | `67:818` | **DEMO - WARMING AFTER PAUSE** | Re-warming at 70% after resume |
| Group 159 | `67:733` | **DEMO - RE-POSITION TIP** | 00:30 cumulative, re-position |
| Group 51 | `37:1674` | **DEMO - END CONFIRMATION** | "Are you sure?" dialog |
| Group 177 | `81:389` | **DEMO - END CONFIRMATION v2** | Duplicate dialog (can be consolidated) |
| Group 174 | `75:320` | **DEMO - COMPLETED SUMMARY** | Back to Home button |
| Group 178 | `81:406` | **DEMO - TREATMENT ENDED** | Final screen after early end |

## 4. REGULAR Treatment Flow (right side of canvas)

| Figma Group | Node ID | Screen Name | Description |
|-------------|---------|-------------|-------------|
| Group 179 | `81:923` | **REG - WARMING UP** | 70% progress, no DEMO badge |
| Group 214 | `102:823` | **REG - POSITION TIP 0:05** | Countdown 5s |
| Group 181 | `81:947` | **REG - POSITION TIP 0:01** | Countdown 1s |
| Group 184 | `81:983` | **REG - TREATMENT ACTIVE** | 25s countdown, 00:05 cumulative |
| Group 185 | `81:1033` | **REG - NEARLY FINISHED** | 00:25 cumulative |
| Group 186 | `81:1077` | **REG - FINAL COUNTDOWN** | Counter at 0 |
| Group 187 | `81:1122` | **REG - END CONFIRMATION** | "Are you sure?" dialog |
| Group 188 | `81:1131` | **REG - END CONFIRMATION v2** | Duplicate (can be consolidated) |
| Group 193 | `81:1265` | **REG - COMPLETED** | 00:30 cumulative, Back to Home |
| Group 194 | `81:1286` | **REG - TREATMENT ENDED** | After early end, Back to Home |
| Group 195 | `81:1310` | **REG - WARMING AFTER PAUSE** | 00:27 cumulative, 70% re-warming |

## 5. Error / Alert Screens

| Figma Element | Node ID | Severity | Description |
|---------------|---------|----------|-------------|
| Rectangle area | `52:2774` | **Informational** | Wi-Fi Temporarily Unavailable (Override mode option) |
| Rectangle area | `46:2334` | **Warning** | Air filter replacement recommended (1000hrs) |
| Rectangle area | `46:2363` | **Warning** | Operating Conditions out of range |
| Rectangle area | `49:2397` | **Critical** | Temperature out of range (prevent further use) |
| Rectangle area | `88:768` | **Critical** | Airflow obstruction detected |
| Rectangle area | `88:787` | **Critical Fault** | Operating temp out of safe range (E010, device shutdown) |

## 6. Reusable Components

| Element | Node ID | Type | Description |
|---------|---------|------|-------------|
| Spinner | `7:8` | COMPONENT | WiFi connecting animation (214×214) |
| bt leave | various | INSTANCE | Back/exit button (60×24) |
| bt Resume click | various | INSTANCE | Resume Treatment button |
| bt continue | various | INSTANCE | Continue/Female button |
| bt End | various | FRAME | End treatment button (98×40) |
| bt Pause | various | FRAME | Pause treatment button (98×40) |

## 7. WiFi Setup Elements (loose)

These elements are scattered in the frame — not grouped into screen-sized containers:
- "Establish Wi-Fi Connection" text
- "Wi-Fi/Network Connection remains unavailable..." warning text
- Override Password entry flow
- WiFi network list (Choose a Network, BTWiFi, etc.)
- "Connected" / "Not connected" status labels

## 8. Design Notes (Sticky Notes in Figma)

| Location | Content Summary |
|----------|----------------|
| Near WiFi screens | UI shows connecting progress with spinner circle. First WiFi setup flow. |
| Near Position Tip | "Not sure what countdown is for — urge operator?" |
| Near Treatment Active | Green reflects flashing light effect. Speaker note for sound. |
| Near Treatment Active | Countdown from 30→0, one dot lights green per second. |
| Near Completed | "Blink and stop at this screen for 3 seconds then jump to next" |
| Near Treatment Ended | Client note: add "Treatment Ended" screen with cumulative time |
| Near Treatment Ended | "After treatment ended, stay for a few seconds then go to home (login?)" |
| Near Regular flow | Treatment Slide #6: When countdown hits 0, loop back to Slide #3 |
| Bottom note | "*Demo mode has not updated yet. Same as regular mode..." |

## 9. Observations & Suggestions

### Duplicates to Consolidate
- **End Confirmation dialogs**: Group 51/177 (Demo) and Group 187/188 (Regular) appear to be duplicates
- **Treatment Pause screen** exists as both a grouped screen (Group 160) and loose elements (Group 159 small, id=81:958)

### Missing from Figma (but exist in code)
- WiFi Settings screen (`screen_wifi_settings.cpp`)
- WiFi Network Details screen (`screen_wifi_network_details.cpp`)
- Password Prompt screen (`screen_password_prompt.cpp`)
- Mode Select screen (`screen_mode_select.cpp`)
- Welcome screen (`screen_welcome.cpp`)

### Organization Issues
- All 206 elements are flat children inside one frame — no sub-sections
- All screens named generically ("Group 47", "Group 197") — no semantic names
- 34 arrows/lines for transitions are loose vectors, not connected via Figma prototyping
- 7+ sticky notes with unresolved design questions
- WiFi setup screens are partially built (scattered elements, not grouped)
- **Note:** Figma REST API is read-only for structure. Renaming groups and restructuring layers must be done manually in the Figma desktop/web app.

### Recommended Figma Actions (Manual)
1. Rename each Group to match the names in this document
2. Create sections/frames for: "Startup", "Demo Flow", "Regular Flow", "Errors", "Components"
3. Move elements into their respective sections
4. Replace arrows with Figma Prototype connections
5. Resolve or archive sticky note questions
6. Delete duplicate end-confirmation screens
