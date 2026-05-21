#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "ui/design_tokens.h"

// ── Existing screens (kept for Wi-Fi & password prompts) ──
#include "screens/screen_wifi_settings.h"
#include "screens/screen_password_prompt.h"

// ── New Figma-aligned screens ──
#include "screens/screen_wifi_init.h"
#include "screens/screen_setup.h"
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
    // The "Not Connected" / override subtree carries an `on_exit` callback:
    // where to land when the user fully backs out of the WiFi list. During
    // boot that is the Setup landing (NOT Home — Home is past Technician
    // Login). When opened from Settings it is Home.
    std::function<void(std::function<void()> on_exit)> show_wifi_unavailable;
    std::function<void(std::function<void()> on_exit)> show_wifi_override_info;
    std::function<void()> show_home;
    // The WiFi list takes a `back` callback so it can return to whichever
    // screen opened it (Home, Settings, …). Self-refreshes inside the screen
    // re-use the same callback.
    std::function<void(std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>>,
                       std::function<void()> /*on_back*/)> show_wifi_setup;
    std::function<void(std::function<void()> on_exit)> show_override_prompt;
    std::function<void(bool demo)> show_login;
    std::function<void(bool demo)> show_patient_info;
    std::function<void(bool demo)> show_demo_info;
    std::function<void(bool demo)> launch_treatment;
    std::function<void(std::function<void()> on_back)> show_settings;
    std::function<void()> show_setup;

    // ── SETUP landing (Figma 151:861) ────────────────────────────────
    // Logo + a "Settings" affordance. This is where Back from the boot-flow
    // WiFi list lands (instead of jumping to Home and skipping login).
    // Tapping Settings opens the device Settings menu; Back there returns
    // to this landing (so we never skip Technician Login).
    show_setup = [&]() {
        printf("[NAV] -> SETUP\n"); fflush(stdout);
        screens.show(create_setup_screen(
            [&]() { show_settings([&]() { show_setup(); }); }
        ));
    };

    // ── WIFI INIT (first boot screen) ────────────────────────────────
    show_wifi_init = [&]() {
        printf("[NAV] -> WIFI_INIT\n"); fflush(stdout);
        screens.show(create_wifi_init_screen(
            [&]() { show_wifi_connected(); },  // on_connected -> Connected gate (manual Continue)
            [&](std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> nets) {
                // on_failed -> WiFi list. Back goes to the Setup landing
                // (151:861), NOT Home — so we don't skip Technician Login.
                show_wifi_setup(nets, [&]() { show_setup(); });
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
            [&]() { show_demo_info(true); },     // Demo Mode -> TRAINING ONLY screen first
            [&]() { show_settings([&]() { show_home(); }); }  // Settings -> Settings menu (Back -> Home)
        ));
    };

    // ── SETTINGS (menu with WiFi + Brightness) ──────────────────────
    // on_back lets the caller decide where Back returns (Home, the Setup
    // landing, the WiFi-unavailable screen, …) so Settings never skips steps.
    show_settings = [&](std::function<void()> on_back) {
        printf("[NAV] -> SETTINGS\n"); fflush(stdout);
        auto back = on_back ? on_back : std::function<void()>([&]() { show_home(); });
        screens.show(create_settings_screen(
            back,                                                          // Back -> caller-chosen
            [&, back]() { show_wifi_setup(nullptr, [&, back](){ show_settings(back); }); }, // WiFi -> WiFi Settings, Back returns here
            [&]() { show_login(false); }                                   // Technician Login -> Login screen
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
            // on_back -> Setup landing (logo + Settings), NOT Home.
            // Backing out of Login must never skip past it into Home.
            [&]() { show_setup(); },
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
        // Demo uses shorter times for quick testing. Warning thresholds
        // are scaled down too so both alerts are reachable within the
        // 30 s demo limit (first at 15 s remaining, second at 5 s).
        if (demo) {
            config.warming_seconds   = 5;
            config.position_tip_seconds = 3;
            config.cycle_seconds     = 10;
            config.process_limit_seconds   = 30;
            config.first_warning_remaining  = 15;
            config.second_warning_remaining = 5;
        }

        TreatmentCallbacks cbs;
        cbs.on_show_screen = [&](std::shared_ptr<egt::Widget> scr) {
            screens.show(scr);
        };
        cbs.on_treatment_completed = [&]() { show_home(); };
        cbs.on_treatment_ended_early = [&]() { show_home(); };
        cbs.on_leave_to_home = [&]() { show_home(); };
        // Hardware alert stubs — log for now, firmware wires these later.
        cbs.on_alert_tone = [](int count) {
            printf("[ALERT] play %d tone(s)\n", count); fflush(stdout);
        };
        cbs.on_tip_led_flash = [](int count) {
            printf("[ALERT] flash tip LED x%d\n", count); fflush(stdout);
        };

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
                    show_wifi_unavailable(back);
                }
            },
            [&](const egt_wifi::WiFiNetwork& net) { (void)net; },
            [&](std::shared_ptr<egt::Widget> scr) { screens.show(scr); },
            cached
        ));
    };

    // ── OVERRIDE PROMPT (existing, kept as-is) ──────────────────────
    // on_exit = where the WiFi-list parent returns to (boot: Setup landing).
    show_override_prompt = [&](std::function<void()> on_exit) {
        printf("[NAV] -> OVERRIDE_PROMPT\n"); fflush(stdout);
        screens.show(create_password_prompt_screen(
            "Override Mode",
            "Enter override password to continue offline",
            "Join", "Back",
            [&, on_exit](const std::string& pass) {
                if (pass == "9999")
                    show_login(false);          // success → Technician Login (never skipped)
                else
                    show_override_prompt(on_exit);
            },
            [&, on_exit]() { show_wifi_override_info(on_exit); }
        ));
    };

    // ── WIFI UNAVAILABLE — "Not Connected" screen (Figma) ────────────
    show_wifi_unavailable = [&](std::function<void()> on_exit) {
        printf("[NAV] -> WIFI_UNAVAILABLE\n"); fflush(stdout);
        screens.show(create_wifi_unavailable_screen(
            // Retry WiFi → back to the network list, keeping the exit target
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); },
            // Setting → open device Settings (Back returns here)
            [&, on_exit]() { show_settings([&, on_exit]() { show_wifi_unavailable(on_exit); }); },
            // Override → existing override-info flow (7-day countdown / password)
            [&, on_exit]() { show_wifi_override_info(on_exit); },
            // Back → previous screen (the WiFi list), keeping the exit target
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); }
        ));
    };

    // ── WIFI OVERRIDE INFO (Figma: WIFI_OVERRIDE_INFO) ──────────────
    show_wifi_override_info = [&](std::function<void()> on_exit) {
        printf("[NAV] -> WIFI_OVERRIDE_INFO\n"); fflush(stdout);
        screens.show(create_wifi_override_info_screen(
            [&, on_exit]() { show_override_prompt(on_exit); },             // Continue -> Override Password
            [&, on_exit]() { show_wifi_unavailable(on_exit); },            // Back -> WiFi Unavailable
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); },         // Retry WiFi -> rescan
            [&, on_exit]() { show_settings([&, on_exit]() { show_wifi_override_info(on_exit); }); } // Setting -> Settings (Back returns here)
        ));
    };

    // ── Boot: start with WiFi Init (or a specific screen for diagnostics) ─
    // EGT_START_SCREEN={settings|home|wifi-settings|login|wifi-unavailable}
    // lets the simulator skip the normal boot flow when iterating on a single
    // screen.
    const char* start = std::getenv("EGT_START_SCREEN");
    if      (start && std::string(start) == "settings")          show_settings([&]() { show_home(); });
    else if (start && std::string(start) == "home")              show_home();
    else if (start && std::string(start) == "wifi-settings")     show_wifi_setup(nullptr, [&]() { show_home(); });
    else if (start && std::string(start) == "wifi-unavailable")  show_wifi_unavailable([&]() { show_home(); });
    else if (start && std::string(start) == "login")             show_login(false);
    else if (start && std::string(start) == "setup")             show_setup();
    else                                                         show_wifi_init();

    win.show();
    app.run();
}