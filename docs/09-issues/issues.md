# Implementation Issues Tracker

Centralized log of issues, gaps, risks, and decisions identified during firmware development.

| Field | Value |
|-------|-------|
| Project | Suntek M4.0 EGT UI |
| Target | SAMA5D27-WLSOM1-EK-SD |
| Status | 1 open, 0 resolved |

---

## ISSUE-001: patient-info step bar does not widen on later steps

- **Type**: [BUG]
- **Symptom**: The green progress underline under the wizard tabs stays the same width on every patient-info step instead of growing as the user advances. Matches on Gender, too narrow on Age.
- **Cause**: The step indicator is drawn at a fixed 133px width for all steps, but Figma sizes it per step (Gender 72 units / 133px, Age 103 units / 191px). Shared code, so demo and real mode are both affected.
- **Refs**:
  - src/screens/screen_patient_info.cpp:437
  - Figma node 142:855 (production Age), node 2009:1173 (demo Age)
- **Fix**: Replace the fixed 133 width with a per-step width array matched to each step's Figma rectangle; also check the ZIP and summary steps against their Figma nodes.
- **Status**: Open
