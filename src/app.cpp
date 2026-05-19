#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "ui/design_tokens.h"

// ── Existing screens (kept for Wi-Fi & password prompts) ──
#include "screens/screen_wifi_settings.h"
#include "screens/screen_password_prompt.h"

// ── New Figma-aligned screens ──
#include "screens/screen_wifi_init.h"
#include "screens/screen_wifi_connected.h"
#include "screens/screen_wifi_connecting.h"
#include "screens/screen_wifi_unavailable.h"
#include "screens/screen_wifi_override_info.h"
#include "screens/screen_home.h"
#include "screens/screen_login_v2.h"
#include "screens/screen_patient_info.h"
#include "screens/screen_demo_info.h"
#include "screens/screen_settings.h"
#include "screens/screen_error.h"

// ── Treatment flow ──
#include "treatment/treatment_controller.h"

// ── WiFi backend ──
#include "wifi/wifi_backend.h"

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);

    // Try Lato as the global font; fall back to system default if Lato is
    // not installed / not in fontconfig cache. This avoids a hard crash on
    // images where Lato hasn't been provisioned yet.
    try {
        egt::global_font(std::make_unique<egt::Font>(
            "Lato", egt::Font::DEFAULT_SIZE, egt::Font::DEFAULT_WEIGHT));
    } catch (...) {
        // keep system default
    }

    egt::TopWindow win;
    ScreenManager screens(win);

    // ── Forward declarations for navigation ──────────────────────────
    std::function<void()> show_wifi_init;
    std::function<void()> show_wifi_connected;
    std::function<void()> show_wifi_unavailable;
    std::function<void()> show_wifi_override_info;
    std::function<void()> show_home;
    // The WiFi list takes a `back` callback so it can return to whichever
    // screen opened it (Home, Settings, …). Self-refreshes inside the screen
    // re-use the same callback.
    std::function<void(std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>>,
                       std::function<void()> /*on_back*/)> show_wifi_setup;
    std::function<void()> show_override_prompt;
    std::function<void(bool demo)> show_login;
    std::function<void(bool demo)> show_patient_info;
    std::function<void(bool demo)> show_demo_info;
    std::function<void(bool demo)> launch_treatment;
    std::function<void()> show_settings;

    // ── WIFI INIT (first boot screen) ────────────────────────────────
    show_wifi_init = [&]() {
        printf("[NAV] -> WIFI_INIT\n"); fflush(stdout);
        screens.show(create_wifi_init_screen(
            [&]() { show_wifi_connected(); },  // on_connected -> Connected gate (manual Continue)
            [&](std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> nets) {
                // on_failed -> WiFi Settings (with pre-scanned nets).
                // Coming from the boot flow → Back should go to Home.
                show_wifi_setup(nets, [&]() { show_home(); });
            },
            [&]() { show_login(false); }   // on_skip -> bypass WiFi, go to Login
        ));
    };

    // ── WIFI CONNECTED (success gate before Login) ──────────────────
    show_wifi_connected = [&]() {
        printf("[NAV] -> WIFI_CONNECTED\n"); fflush(stdout);
        screens.show(create_wifi_connected_screen(
            [&]() { show_login(false); }   // Continue -> Technician Login
        ));
    };

    // ── HOME ─────────────────────────────────────────────────────────
    show_home = [&]() {
        printf("[NAV] -> HOME\n"); fflush(stdout);
        screens.show(create_home_screen(
            [&]() { show_patient_info(false); }, // Begin Treatment -> Patient Info
            [&]() { show_patient_info(true); },   // Demo Mode -> Patient Info (demo)
            [&]() { show_settings(); }           // Settings -> Settings menu
        ));
    };

    // ── SETTINGS (menu with WiFi + Brightness) ──────────────────────
    show_settings = [&]() {
        printf("[NAV] -> SETTINGS\n"); fflush(stdout);
        screens.show(create_settings_screen(
            [&]() { show_home(); },                                  // Back -> Home
            [&]() { show_wifi_setup(nullptr, [&](){ show_settings(); }); }  // WiFi -> WiFi Settings, Back returns here
        ));
    };

    // ── LOGIN (card grid) ────────────────────────────────────────────
    // Default technician list
    std::vector<TechnicianProfile> technicians = {
        {"Alice", "1234"},
        {"Bob",   "5678"},
        {"Carol", "0000"},
        {"Dave",  "1111"},
        {"Eve",   "2222"},
    };

    show_login = [&](bool demo) {
        printf("[NAV] -> LOGIN (demo=%d)\n", demo); fflush(stdout);
        screens.show(create_login_screen_v2(
            technicians,
            [&, demo](const std::string& user) { // on_login_success
                printf("Logged in as: %s\n", user.c_str());
                show_home();
            },
            [&]() { show_home(); }, // on_back
            [&](std::shared_ptr<egt::Widget> scr) { // on_show_screen
                screens.show(scr);
            }
        ));
    };

    // ── DEMO INFO ────────────────────────────────────────────────────
    show_demo_info = [&](bool demo) {
        printf("[NAV] -> DEMO_INFO\n"); fflush(stdout);
        screens.show(create_demo_info_screen(
            [&]() { show_patient_info(true); }, // Continue -> Patient Info (demo)
            [&]() { show_home(); }              // Back -> Home
        ));
    };

    // ── PATIENT INFO (3-step wizard) ─────────────────────────────────
    show_patient_info = [&](bool demo) {
        printf("[NAV] -> PATIENT_INFO (demo=%d)\n", demo); fflush(stdout);
        screens.show(create_patient_info_screen(
            demo,
            [&, demo](const PatientInfo& info) { // on_complete
                printf("Patient: gender=%s age=%d zip=%s\n",
                    info.gender.c_str(), info.age, info.zip_code.c_str());
                launch_treatment(demo);
            },
            [&]() { show_home(); }, // on_back
            [&](std::shared_ptr<egt::Widget> scr) { // on_show_screen
                screens.show(scr);
            },
            [&]() { show_home(); }  // on_leave_demo
        ));
    };

    // ── TREATMENT FLOW ───────────────────────────────────────────────
    launch_treatment = [&](bool demo) {
        printf("[NAV] -> TREATMENT (demo=%d)\n", demo); fflush(stdout);
        TreatmentConfig config;
        config.demo_mode = demo;
        // Demo uses shorter times for quick testing
        if (demo) {
            config.warming_seconds   = 5;
            config.position_tip_seconds = 3;
            config.cycle_seconds     = 10;
            config.total_target_seconds = 30;
        }

        TreatmentCallbacks cbs;
        cbs.on_show_screen = [&](std::shared_ptr<egt::Widget> scr) {
            screens.show(scr);
        };
        cbs.on_treatment_completed = [&]() { show_home(); };
        cbs.on_treatment_ended_early = [&]() { show_home(); };
        cbs.on_leave_to_home = [&]() { show_home(); };

        start_treatment_flow(config, cbs);
    };

    // ── WI-FI SETTINGS (existing screen, kept as-is) ────────────────
    show_wifi_setup = [&](std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> cached,
                          std::function<void()> on_back) {
        // Capture the back callback so any self-refresh (auto-scan, connect
        // bounce-back, etc.) keeps returning to the same parent screen.
        auto back = on_back;
        screens.show(create_wifi_settings_panel(
            back,                                                          // on_back
            [&, back]() { show_wifi_setup(nullptr, back); },               // on_scan_wifi (refresh)
            [&, back](const std::string& ssid, const std::string& password) {
                if (!ssid.empty() && !password.empty()) {
                    // Async connect on a dedicated "Connecting..." screen so the
                    // UI never freezes during the (blocking) associate + DHCP.
                    printf("[WIFI] Connecting to '%s'...\n", ssid.c_str());
                    fflush(stdout);
                    screens.show(create_wifi_connecting_screen(
                        ssid, password,
                        [&]() {                       // on_success
                            printf("[WIFI] Connected!\n"); fflush(stdout);
                            show_wifi_connected();
                        },
                        [&, back]() {                 // on_failure → bounce back to list with same Back target
                            printf("[WIFI] Connection failed\n"); fflush(stdout);
                            show_wifi_setup(nullptr, back);
                        }
                    ));
                } else {
                    show_wifi_unavailable();
                }
            },
            [&](const egt_wifi::WiFiNetwork& net) { (void)net; },
            [&](std::shared_ptr<egt::Widget> scr) { screens.show(scr); },
            cached
        ));
    };

    // ── OVERRIDE PROMPT (existing, kept as-is) ──────────────────────
    show_override_prompt = [&]() {
        printf("[NAV] -> OVERRIDE_PROMPT\n"); fflush(stdout);
        screens.show(create_password_prompt_screen(
            "Override Mode",
            "Enter override password to continue offline",
            "Join", "Back",
            [&](const std::string& pass) {
                if (pass == "9999")
                    show_login(false);
                else
                    show_override_prompt();
            },
            [&]() { show_wifi_override_info(); }
        ));
    };

    // ── WIFI UNAVAILABLE — "Not Connected" screen (Figma) ────────────
    show_wifi_unavailable = [&]() {
        printf("[NAV] -> WIFI_UNAVAILABLE\n"); fflush(stdout);
        screens.show(create_wifi_unavailable_screen(
            // Retry WiFi → go back to the network list (will re-scan)
            [&]() { show_wifi_setup(nullptr, [&]() { show_home(); }); },
            // Setting → open device Settings
            [&]() { show_settings(); },
            // Override → existing override-info flow (7-day countdown / password)
            [&]() { show_wifi_override_info(); },
            // Back → previous screen (the WiFi list)
            [&]() { show_wifi_setup(nullptr, [&]() { show_home(); }); }
        ));
    };

    // ── WIFI OVERRIDE INFO (Figma: WIFI_OVERRIDE_INFO) ──────────────
    show_wifi_override_info = [&]() {
        printf("[NAV] -> WIFI_OVERRIDE_INFO\n"); fflush(stdout);
        screens.show(create_wifi_override_info_screen(
            [&]() { show_override_prompt(); },    // Continue -> Override Password
            [&]() { show_wifi_unavailable(); }    // Back -> WiFi Unavailable
        ));
    };

    // ── Boot: start with WiFi Init (or a specific screen for diagnostics) ─
    // EGT_START_SCREEN={settings|home|wifi-settings|login|wifi-unavailable}
    // lets the simulator skip the normal boot flow when iterating on a single
    // screen.
    const char* start = std::getenv("EGT_START_SCREEN");
    if      (start && std::string(start) == "settings")          show_settings();
    else if (start && std::string(start) == "home")              show_home();
    else if (start && std::string(start) == "wifi-settings")     show_wifi_setup(nullptr, [&]() { show_home(); });
    else if (start && std::string(start) == "wifi-unavailable")  show_wifi_unavailable();
    else if (start && std::string(start) == "login")             show_login(false);
    else                                                         show_wifi_init();

    win.show();
    app.run();
}