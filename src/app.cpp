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
#include "screens/screen_wifi_not_found.h"
#include "screens/screen_wifi_override_intro.h"
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
        {"Angelina Jolie",     "1234"},
        {"Denzel Washington",  "5678"},
        {"Leonardo DiCaprio",  "0000"},
        {"Meryl Streep",       "1111"},
        {"Scarlett Johansson", "2222"},
        {"Vanessa Hudgens",    "3333"},
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
                    // No SSID/password supplied → show the "Wi-Fi Network
                    // not found" intro (3 buttons) instead of jumping
                    // straight to the override-info screen.
                    screens.show(create_wifi_not_found_screen(
                        [&, back]() { show_wifi_unavailable(back); },         // Operate without WiFi -> override info
                        [&, back]() { show_wifi_setup(nullptr, back); },      // Retry WiFi -> rescan
                        [&, back]() { show_settings(back); }                   // Setting -> Settings menu
                    ));
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

    // ── WIFI UNAVAILABLE / OVERRIDE INFO — single Figma screen (2079:2300)
    // with an optional explanatory popup overlay. Two entry points share the
    // same factory; the only difference is whether the popup is open on
    // first render.
    std::function<void(std::function<void()>, bool)> show_wifi_unavail;

    // Intermediate "Enter Override password" intro card (Figma 134:1421). Sits
    // between WiFi Unavailable's Continue and the actual password keypad so
    // the user explicitly opts in before typing.
    std::function<void(std::function<void()>)> show_wifi_override_intro;

    show_wifi_unavail = [&](std::function<void()> on_exit, bool show_popup) {
        screens.show(create_wifi_unavailable_screen(
            [&, on_exit]() { show_wifi_override_intro(on_exit); },          // Continue -> Override Intro
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); },          // Back -> WiFi list
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); },          // Retry WiFi -> rescan
            [&, on_exit]() { show_settings([&, on_exit]() {                 // Setting -> Settings (Back returns here, no popup)
                show_wifi_unavail(on_exit, false);
            }); },
            show_popup
        ));
    };

    show_wifi_override_intro = [&](std::function<void()> on_exit) {
        printf("[NAV] -> WIFI_OVERRIDE_INTRO\n"); fflush(stdout);
        screens.show(create_wifi_override_intro_screen(
            [&, on_exit]() { show_override_prompt(on_exit); },              // Enter Override -> password keypad
            [&, on_exit]() { show_wifi_unavail(on_exit, false); },          // Back -> WiFi Unavailable
            [&, on_exit]() { show_wifi_setup(nullptr, on_exit); },          // Retry WiFi -> rescan
            [&, on_exit]() { show_settings([&, on_exit]() {                 // Setting -> Settings (Back returns to intro)
                show_wifi_override_intro(on_exit);
            }); }
        ));
    };

    show_wifi_unavailable = [&](std::function<void()> on_exit) {
        printf("[NAV] -> WIFI_UNAVAILABLE\n"); fflush(stdout);
        show_wifi_unavail(on_exit, /*show_popup=*/false);
    };

    show_wifi_override_info = [&](std::function<void()> on_exit) {
        printf("[NAV] -> WIFI_OVERRIDE_INFO\n"); fflush(stdout);
        show_wifi_unavail(on_exit, /*show_popup=*/true);
    };

    // ── Boot: start with WiFi Init (or a specific screen for diagnostics) ─
    // EGT_START_SCREEN={settings|home|wifi-settings|login|wifi-unavailable|demo-info|password
    //                   |treatment|treatment-demo|error-temp|error-filter
    //                   |warning-temp|warning-airflow|fault-critical}
    // lets the simulator skip the normal boot flow when iterating on a single
    // screen. For treatment, pair with EGT_MOCK_TREATMENT=<screen> to hold on
    // a specific sub-screen (warming|ready|position|reposition|active|nearly|
    // paused|end-confirm|completed|ended).
    const char* start = std::getenv("EGT_START_SCREEN");
    if      (start && std::string(start) == "settings")          show_settings([&]() { show_home(); });
    else if (start && std::string(start) == "home")              show_home();
    else if (start && std::string(start) == "wifi-settings") {
        // Inject a fixed dummy list so the screen renders predictably for
        // figma-vs-sim screenshots, bypassing the live scan + mock timing.
        // Signals chosen so the natural sort matches the Figma render order.
        auto dummy = std::make_shared<std::vector<egt_wifi::WiFiNetwork>>();
        dummy->push_back({"BTWiFi",          90, "", false});
        dummy->push_back({"BTWiFi-With-Fon", 80, "", true });
        dummy->push_back({"John's iMac",     65, "", false});
        show_wifi_setup(dummy, [&]() { show_home(); });
    }
    // wifi-unavailable now takes on_exit (subtree was rethreaded so Back
    // never skips Login). Diagnostic launches just send it to Home.
    else if (start && std::string(start) == "wifi-unavailable")  show_wifi_unavailable([&]() { show_home(); });
    else if (start && std::string(start) == "wifi-override-intro") show_wifi_override_intro([&]() { show_home(); });
    else if (start && std::string(start) == "wifi-not-found") {
        screens.show(create_wifi_not_found_screen(
            [&]() { show_wifi_unavailable([&]() { show_home(); }); },
            [&]() { show_wifi_setup(nullptr, [&]() { show_home(); }); },
            [&]() { show_settings([&]() { show_home(); }); }
        ));
    }
    else if (start && std::string(start) == "login")             show_login(false);
    else if (start && std::string(start) == "setup")             show_setup();
    else if (start && std::string(start) == "demo-info")         show_demo_info(true);
    else if (start && std::string(start) == "password") {
        screens.show(create_password_prompt_screen(
            "Enter Password", "User: Leonardo DiCaprio",
            "Join", "Back",
            [&](const std::string&) { show_home(); },
            [&]() { show_login(false); }));
    }
    else if (start && std::string(start) == "wifi-connecting") {
        screens.show(create_wifi_connecting_screen(
            "TestNetwork", "password",
            [&]() { show_wifi_connected(); },
            // unavailable now takes an on_exit (subtree was rethreaded so Back
            // never skips Login). Diagnostic launches just send it to Home.
            [&]() { show_wifi_unavailable([&]() { show_home(); }); }));
    }
    else if (start && std::string(start) == "wifi-override-info") show_wifi_override_info([&]() { show_home(); });
    else if (start && std::string(start) == "patient-info")       show_patient_info(false);
    else if (start && std::string(start) == "patient-info-demo")  show_patient_info(true);
    else if (start && std::string(start) == "treatment")          launch_treatment(false);
    else if (start && std::string(start) == "treatment-demo")     launch_treatment(true);
    else if (start && std::string(start) == "error-temp") {
        screens.show(create_error_screen(
            "assets/figma/icons/error-icon-temp.svg",
            "Operating Conditions out of range",
            "Only operating the device above the\nrecommended temperature.",
            {{"Pause", "", [&]() { show_home(); }},
             {"Resume", "", [&]() { show_home(); }},
             {"End", "", [&]() { show_home(); }}}));
    }
    else if (start && std::string(start) == "error-filter") {
        screens.show(create_error_screen(
            "assets/figma/icons/error-icon-filter.svg",
            "Change the air filter",
            "Air filter replacement recommended - 1000\nhours of use reached.",
            {{"Pause", "", [&]() { show_home(); }},
             {"Begin", "Treatment", [&]() { show_home(); }},
             {"End", "", [&]() { show_home(); }}}));
    }
    else if (start && std::string(start) == "warning-temp") {
        screens.show(create_error_screen(
            "assets/figma/icons/warning-icon-temp.svg",
            "Temperature out of range",
            "Prevent further use of the machine to avoid\nineffective treatments.",
            {}, egt::Color(0xFF, 0x9E, 0x1B)));
    }
    else if (start && std::string(start) == "warning-airflow") {
        screens.show(create_error_screen(
            "assets/figma/icons/warning-icon-airflow.svg",
            "Airflow obstruction detected",
            "Please change the air filter before further use.",
            {}, egt::Color(0xFF, 0x9E, 0x1B)));
    }
    else if (start && std::string(start) == "fault-critical") {
        screens.show(create_error_screen(
            "assets/figma/icons/critical-icon-alert.svg",
            "Operating temperature out of safe range",
            "Device has been shut down to prevent hazard.\n"
            "Contact support before restarting.\nError Code: E010",
            {}, egt::Color(0xE6, 0x3C, 0x16)));
    }
    else                                                         show_wifi_init();

    win.show();
    app.run();
}