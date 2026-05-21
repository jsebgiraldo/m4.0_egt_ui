# Suntek M4 EGT UI — Documentation

Embedded UI for the Suntek M4 treatment device. This folder is the documentation hub. Each topic lives in its own numbered subfolder. To add a new doc, use the `new-doc` Claude skill (see `.claude/skills/new-doc/SKILL.md`) — it places the file in the right folder and updates this index.

> See the repository [README.md](../README.md) for build instructions and the [Figma design source](../README.md#design-source).

## Folder map

| # | Folder | What goes here |
|---|---|---|
| 01 | [requirements](01-requirements/) | Product requirements, feature scope, UX flows |
| 02 | [architecture](02-architecture/) | High-level architecture, screen navigation, controller state machines |
| 03 | [features](03-features/) | Per-feature design notes (WiFi flow, treatment controller, overrides, etc.) |
| 04 | [ide-setup](04-ide-setup/) | Dev environment, simulator, VSCode workspace, debugging |
| 05 | [hardware](05-hardware/) | SAMA5D27 panel, input devices, GPIO usage, peripheral notes |
| 06 | [testing](06-testing/) | Visual checkpoints, manual test scripts, on-target verification |
| 07 | [deployment](07-deployment/) | Cross-compile, Yocto image, deploy-to-target, service management |
| 08 | [references](08-references/) | External docs, datasheets, EGT framework links, Figma exports |
| 09 | [issues](09-issues/) | Bug tracker, Figma-vs-code gap analysis, iteration reports |

## Index

### 01 — Requirements
*(No documents yet)*

### 02 — Architecture
*(No documents yet)*

### 03 — Features
*(No documents yet)*

### 04 — IDE setup
- [simulator.md](04-ide-setup/simulator.md) — Host simulator walkthrough (build, flags, WSL2 setup, troubleshooting)

### 05 — Hardware
*(No documents yet)*

### 06 — Testing
*(No documents yet)*

### 07 — Deployment
- [useful-commands.md](07-deployment/useful-commands.md) — Yocto cross-compile + scp deploy commands

### 08 — References
*(No documents yet)*

### 09 — Issues
- [check.md](09-issues/check.md) — Live UI/UX TODO list (Spanish, Figma node references)
- [m4-18-wifi-init-figma-match/](09-issues/m4-18-wifi-init-figma-match/) — Figma-vs-code iteration report for WIFI_INIT screen (Jira M4-18)

## Conventions

- Filenames in **kebab-case** (no spaces, no uppercase): `wifi-flow-overview.md`
- Each doc starts with a `# Title` heading, an "Overview" section, and links its references at the bottom
- New files must live inside a numbered folder, never at `docs/` root
- Images that belong to a single doc can live next to it; reusable images go in `docs/img/`
