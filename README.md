# Suntek M4 EGT Application

Embedded UI for the Suntek M4 treatment device, built with [Ensemble Graphics Toolkit (EGT)](https://github.com/linux4sam/egt) v1.12.1.

**Target:** SAMA5D27-WLSOM1-EK-SD (ARM Cortex-A5, 800×480 display)

## Project Structure

```
src/
├── main.cpp                   # Entry point
├── app.cpp / app.h            # Application flow & screen navigation
├── screen_manager.cpp / .h    # TopWindow widget manager
├── ui/
│   ├── design_tokens.h        # Colors, fonts, dimensions (from Figma)
│   └── components.cpp / .h    # Shared UI components (buttons, progress, footer)
├── screens/
│   ├── screen_home.*          # Home screen (Begin Treatment / Demo / Settings)
│   ├── screen_login_v2.*     # Technician login (2×3 card grid)
│   ├── screen_patient_info.* # 4-step patient wizard (gender→age→zip→confirm)
│   ├── screen_demo_info.*    # Demo mode info notice
│   ├── screen_wifi_init.*    # Spinner shown at boot while WiFi connects
│   ├── screen_wifi_settings.*# WiFi scan & connect (real APs via nmcli or wpa_cli)
│   ├── screen_wifi_connecting.* # "Connecting to <SSID>" spinner (async)
│   ├── screen_wifi_connected.*  # Success gate: ✓ + Continue button
│   ├── screen_wifi_unavailable.*# Override-mode entry point if WiFi fails
│   ├── screen_password_prompt.* # QWERTY keyboard password entry
│   └── _legacy/              # Unused legacy screens (not compiled)
├── treatment/
│   └── treatment_controller.* # Full treatment state machine (6 sub-screens)
└── wifi/
    └── wifi_backend.*         # nmcli WiFi scanning/connecting
```

## Quick Start

### 1. Native Build (recommended for simulator)

Install EGT and dependencies:
```bash
# Install build deps (Ubuntu 24.04)
sudo apt install cmake g++ libcairo-dev libdrm-dev libinput-dev \
    libxkbcommon-dev librsvg2-dev liblua5.3-dev libcurl4-openssl-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libjpeg-dev libmagic-dev libplplot-dev libasound2-dev libsndfile1-dev \
    libnm-dev libdbus-1-dev libx11-dev

# Clone and build EGT
sudo git clone --recursive https://github.com/linux4sam/egt.git /opt/egt
cd /opt/egt && sudo mkdir build && cd build
sudo cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && sudo make -j$(nproc) && sudo make install && sudo ldconfig
```

Build and run the app:
```bash
./scripts/run-simulator.sh --build
```

That configures `build-x86/`, builds, and opens an 800×480 X11 window with `EGT_MOCK_WIFI=1` (dynamic mock APs). Subsequent runs without `--build` just relaunch. See [docs/SIMULATOR.md](docs/SIMULATOR.md) for the full walkthrough — flags, WSL2 setup, troubleshooting, and what to look for in each screen.

Manual equivalent:
```bash
mkdir -p build-x86 && cd build-x86
cmake .. && make -j$(nproc)
EGT_BACKEND=x11 EGT_SCREEN_SIZE=800x480 EGT_MOCK_WIFI=1 ./egt-app
```

### 2. Docker Build

```bash
docker build -t egt-app-dev .
docker run --rm -v "$PWD:/app" -w /app egt-app-dev \
    bash -c "mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
```

### 3. Cross-Compile (Yocto / target hardware)

```bash
./scripts/cross-build.sh
```

## Application Flow

```
Boot
 │
 ▼
WIFI_INIT (spinner)
 ├── connected ──► WIFI_CONNECTED (✓ + Continue) ──► LOGIN
 └── no nets   ──► WIFI_SETTINGS (list)
                    │
                    └── tap AP → password → WIFI_CONNECTING (spinner)
                                              │
                                              ├── OK   → WIFI_CONNECTED → LOGIN
                                              └── fail → WIFI_SETTINGS (retry)

LOGIN → Patient Info (4 steps) → Treatment ── End / Complete ──► HOME
HOME → Demo Info → Patient Info → Treatment (short timings)
HOME → Settings (WiFi)
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `EGT_BACKEND` | auto (x11 > kms) | Display backend: `x11`, `kms`, `memory` |
| `EGT_SCREEN_SIZE` | `800x480` | Window size for X11/SDL backends |
| `EGT_MOCK_WIFI` | _(unset on target, `1` in simulator)_ | `1` = dynamic mock APs; `connected` = jump straight to WIFI_CONNECTED; unset = real WiFi via `nmcli` → `wpa_cli` fallback |

## Documentation

- **[docs/SIMULATOR.md](docs/SIMULATOR.md)** — set up & run the x86 simulator (WSL/Linux), flags, troubleshooting, per-screen walkthrough.
- **[docs/useful-commands.md](docs/useful-commands.md)** — common Docker / cross-compile / deploy commands.
