#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "ui/design_tokens.h"

// ── Existing screens (kept for Wi-Fi & password prompts) ──
#include "screens/screen_wifi_settings.h"
#include "screens/screen_password_prompt.h"

// ── New Figma-aligned screens ──
#include "screens/screen_wifi_init.h"
#include "screens/screen_home.h"
#include "screens/screen_login_v2.h"
#include "screens/screen_patient_info.h"
#include "screens/screen_demo_info.h"

// ── Treatment flow ──
#include "treatment/treatment_controller.h"

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    // ── Forward declarations for navigation ──────────────────────────
    std::function<void()> show_wifi_init;
    std::function<void()> show_home;
    std::function<void()> show_wifi_setup;
    std::function<void()> show_override_prompt;
    std::function<void(bool demo)> show_login;
    std::function<void(bool demo)> show_patient_info;
    std::function<void(bool demo)> show_demo_info;
    std::function<void(bool demo)> launch_treatment;

    // ── WIFI INIT (first boot screen) ────────────────────────────────
    show_wifi_init = [&]() {
        printf("[NAV] -> WIFI_INIT\n"); fflush(stdout);
        screens.show(create_wifi_init_screen(
            [&]() { show_login(false); },  // on_connected -> Technician Login
            [&]() { show_wifi_setup(); }   // on_failed -> WiFi Settings
        ));
    };

    // ── HOME ─────────────────────────────────────────────────────────
    show_home = [&]() {
        printf("[NAV] -> HOME\n"); fflush(stdout);
        screens.show(create_home_screen(
            [&]() { show_login(false); },    // Begin Treatment -> Login
            [&]() { show_demo_info(true); }, // Demo Mode -> Demo Info
            [&]() { show_wifi_setup(); }     // Settings -> Wi-Fi
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
                show_patient_info(demo);
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
    show_wifi_setup = [&]() {
        screens.show(create_wifi_settings_panel(
            [&]() { show_home(); },            // on_back
            [&]() { show_wifi_setup(); },      // on_scan_wifi (refresh)
            [&](const std::string& ssid, const std::string& password) {
                if (!ssid.empty() && !password.empty())
                    show_home();
                else
                    show_override_prompt();
            },
            [&](const egt_wifi::WiFiNetwork& net) { (void)net; },
            [&](std::shared_ptr<egt::Widget> scr) { screens.show(scr); }
        ));
    };

    // ── OVERRIDE PROMPT (existing, kept as-is) ──────────────────────
    show_override_prompt = [&]() {
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
            [&]() { show_home(); }
        ));
    };

    // ── Boot: start with WiFi Init ───────────────────────────────────
    show_wifi_init();

    win.show();
    app.run();
}