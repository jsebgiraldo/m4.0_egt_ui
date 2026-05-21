# Running the EGT App Simulator (x86 host)

Step-by-step guide to run the app on a Linux/WSL2 desktop without the SAMA hardware. Useful for UI iteration, design review, and debugging without burning deploy cycles.

The simulator is a normal X11 window at the target resolution (800×480). All UI logic is the same code that runs on the SAMA — only the rendering backend changes.

---

## 1. Prerequisites

- **OS**: Ubuntu 22.04 / 24.04, Debian 12, or WSL2 (Ubuntu 22.04+) with WSLg
- **EGT 1.12+** installed on the host
- Build essentials: `cmake`, `g++`, `pkg-config`
- An X server (native Linux: automatic; WSL2: WSLg ships built-in)

### Install host EGT once

> **Order matters.** Several EGT features (SVG support, Lua bindings, plplot, etc.) are conditional on optional dev libs being present at the time `cmake` runs. Install the apt deps **first**, then build EGT. If you skip a dep and rebuild later, the relevant headers (e.g. `egt/svgimage.h`) won't be in `/usr/local/include/egt/` — see Troubleshooting.

```bash
sudo apt update
sudo apt install -y cmake g++ pkg-config \
  libcairo-dev libdrm-dev libinput-dev libxkbcommon-dev \
  librsvg2-dev liblua5.3-dev libcurl4-openssl-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libjpeg-dev libmagic-dev libplplot-dev libasound2-dev libsndfile1-dev \
  libnm-dev libdbus-1-dev libx11-dev gettext

sudo git clone --recursive https://github.com/linux4sam/egt.git /opt/egt
cd /opt/egt && sudo mkdir build && cd build
sudo cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo make -j$(nproc)
sudo make install
sudo ldconfig

# Verify
pkg-config --modversion libegt   # → 1.12.1 (or newer)
```

---

## 2. One-command launch

From the repo root:

```bash
./scripts/run-simulator.sh --build
```

The first run configures CMake, compiles, and opens an 800×480 window. Subsequent runs (no flag) reuse the existing build.

### Useful flags

| Flag | Effect |
|---|---|
| (none) | Use existing `build-x86/`, just relaunch |
| `--build` | Reconfigure + incremental build, then launch |
| `--clean` | Nuke `build-x86/` and rebuild from scratch |
| `--wifi-static` | Force `EGT_MOCK_WIFI=connected` — skips the WiFi list and shows the **Wi-Fi Connected** success screen directly. Handy for design review of that one screen. |

### Manual launch (equivalent)

```bash
mkdir -p build-x86 && cd build-x86
cmake .. && make -j$(nproc)
EGT_BACKEND=x11 EGT_SCREEN_SIZE=800x480 EGT_MOCK_WIFI=1 ./egt-app
```

---

## 3. WSL2 / WSLg notes

WSL2 + Windows 11 ships WSLg — X11 and Wayland apps render natively to the Windows desktop. `DISPLAY=:0` is set automatically and `/tmp/.X11-unix/X0` exists.

Sanity check from your WSL shell:

```bash
echo $DISPLAY               # should print :0
ls /tmp/.X11-unix/          # should contain X0 (or X1)
xeyes                       # if installed: window pops up on Windows
```

If `xeyes` doesn't appear, WSLg isn't healthy. Recover with `wsl --shutdown` from a Windows PowerShell, then reopen WSL.

On **WSL1** or older WSL2 without WSLg you need a separate X server (VcXsrv, X410) and `export DISPLAY=$(grep nameserver /etc/resolv.conf | awk '{print $2}'):0`.

---

## 4. Environment variables

| Variable | Default | Effect |
|---|---|---|
| `EGT_BACKEND` | `x11` (set by script) | Rendering backend. `x11` for desktop, `kms` for DRM (target). |
| `EGT_SCREEN_SIZE` | `800x480` | Window dimensions. Must match design tokens — don't change unless you also adjust `dt::SCREEN_W/H`. |
| `EGT_MOCK_WIFI` | `1` | WiFi backend mode (see table below). |
| `DISPLAY` | `:0` | X server to connect to. |

### `EGT_MOCK_WIFI` modes

| Value | Effect |
|---|---|
| _(unset)_ | Real WiFi — queries `nmcli` first, falls back to `wpa_cli` if NM doesn't manage `wlan0`. On a typical desktop neither is set up, so the app shows an empty list / fails. |
| `1` | **Dynamic mock** (recommended for UI iteration). 12 fake APs (`SunTek-Office`, `Familia Giraldo`, etc.), signal strengths wobble each scan, one AP rotates in/out as "transient", one rotates as "connected" every 10 ticks. |
| `connected` | Backend reports already-connected to `SunTek-Office`. App boots straight into the **Wi-Fi Connected** screen — perfect for reviewing that exact screen without traversing the flow. |

---

## 5. What to look for — flow & UX checkpoints

The simulator renders the full app flow. After launch you'll land on:

```
WIFI_INIT (spinner) → if mock=1 → no saved nets → scan → WIFI_SETTINGS (list)
                                                          │
                  tap a network → password prompt → Join  ▼
                                                  WIFI_CONNECTING (spinner)
                                                          │
                                  mock returns OK (~5s)   ▼
                                                  WIFI_CONNECTED (✓)
                                                          │
                                                   Continue ▼
                                                       LOGIN ...
```

### UX checkpoints introduced in the recent passes

- **WIFI_SETTINGS scan spinner** — small rotating arc next to "Choose a Network..." while a scan thread runs.
- **Scroll preservation** — scroll the list down with arrow keys (↑/↓) or drag; refreshes every 2 s should **not** jump you back to the top.
- **Connected-first sort** — when the dynamic mock rotates an AP to `connected=true`, it should pin to the top of the list and turn green.
- **Incremental updates without flicker** — in real-WiFi mode the SSID set is stable so the cheap in-place update path runs (no row remove/re-add). Logs show `signal/state update in place` vs `full rebuild`.
- **WIFI_CONNECTING screen** — green rotating ring + "Connecting to \<SSID\>" + green SSID label. On failure, a red status message appears for ~2 s before bouncing back.
- **WIFI_CONNECTED screen** — small logo top-left, green check circle, "Wi-Fi Connected" + subtitle + outlined `Continue >` button.

### Quick keyboard shortcuts inside the simulator window

| Key | Action |
|---|---|
| ↑ / ↓ | Scroll the WiFi list |
| Mouse drag | Touch drag (simulates the panel touchscreen) |
| Ctrl-C in launching terminal | Quit |

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| `error while loading shared libraries: libegt.so.X` | Run `sudo ldconfig` after installing EGT. |
| `fatal error: egt/svgimage.h: No such file or directory` | EGT was built before `librsvg2-dev` was installed, so SVG support was disabled and the header wasn't installed. Install `librsvg2-dev`, then rebuild EGT: `sudo bash -c "rm -rf /opt/egt/build && mkdir /opt/egt/build && cd /opt/egt/build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && make -j\$(nproc) && make install && ldconfig"`. Verify with `ls /usr/local/include/egt/svgimage.h`. The same trap applies to other optional EGT features (`WITH_LIBRSVG`, `WITH_LUA_BINDINGS`, etc.) — install all apt deps **before** building EGT. |
| `CMake Error: source "/app" does not exist` | Stale CMakeCache from a previous Docker build. `rm -rf build-x86 && ./scripts/run-simulator.sh --clean`. |
| Window opens then immediately closes | Inspect terminal output. Common: missing asset (`assets/image/lice-temp-logo.png`) — make sure you ran from the repo root. |
| Fontconfig warnings about missing Lato | Non-fatal; app falls back to the system font (handled by a `try/catch` in `app.cpp`). |
| `nmcli: not found` log line | Expected on host. Backend logs this once before falling through to mock (`EGT_MOCK_WIFI=1`) or wpa_cli. |
| Window doesn't appear on WSL | `wsl --shutdown` from a Windows PowerShell, reopen WSL, re-run. WSLg state can get stuck. |
| Need a different screen size | `EGT_SCREEN_SIZE=1280x800 ./scripts/run-simulator.sh` — but UI is hard-coded to 800×480 in `dt::SCREEN_W/H`, so anything else clips or stretches. |

---

## 7. Reproducing a specific screen quickly

| Screen | How |
|---|---|
| WIFI_INIT spinner | Default launch — the first ~2 s show this. |
| WIFI_SETTINGS list | Default launch — after the ~2 s timeout. |
| Password prompt | Tap any row in the WIFI_SETTINGS list. |
| WIFI_CONNECTING | Tap a row → enter any text → Join. Mock returns OK after ~5 s. |
| WIFI_CONNECTED | `./scripts/run-simulator.sh --wifi-static` (skips the flow). |
| LOGIN / HOME / Patient Info / Treatment | Tap **Continue** on WIFI_CONNECTED, then follow the flow. |

---

## 8. Iterating on UI code

1. Edit a `src/screens/screen_*.cpp` file.
2. Re-run `./scripts/run-simulator.sh --build` — incremental build, usually <10 s.
3. Window relaunches with the change.

No need to redeploy to the SAMA for visual tweaks — match the layout in the simulator first, then cross-compile + flash only once the design is locked.
