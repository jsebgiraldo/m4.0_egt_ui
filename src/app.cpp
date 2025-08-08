#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "screens/screen_start.h"
#include "screens/screen_welcome.h"
#include "screens/screen_login.h"
#include "screens/screen_wifi_settings.h"
#include "screens/screen_wifi_network_details.h"
#include "screens/screen_mode_select.h"
#include "screens/screen_training.h"
#include "screens/screen_password_prompt.h"  // ← AGREGAR ESTA LÍNEA

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    std::function<void()> show_login;
    std::function<void()> show_mode_select;
    std::function<void()> show_wifi_setup;
    std::function<void()> show_override_prompt;

    // Technical login (after Wi-Fi or override)
    show_login = [&]() {
        screens.show(create_login_screen(
            [&]() { // on_success
                show_mode_select();
            },
            [&]() { // on_cancel
                app.quit();
            }
        ));
    };

    // Mode select
    show_mode_select = [&]() {
        screens.show(create_mode_select_screen(
            [&]() { // on_training
                screens.show(create_training_screen(
                    300, // 5 minutos de duración
                    [&]() { // on_complete
                        show_mode_select();
                    },
                    [&]() { // on_pause
                        // Lógica adicional de pausa si es necesaria
                    },
                    [&]() { // on_cancel
                        show_mode_select();
                    }
                ));
            },
            [&]() { // on_treatment
                screens.show(create_training_screen(
                    600, // 10 minutos de duración para tratamiento
                    [&]() { show_mode_select(); },
                    [&]() { /* pausa */ },
                    [&]() { show_mode_select(); }
                ));
            },
            [&]() { // on_back
                show_login();
            }
        ));
    };

    // Override prompt usando pantalla genérica
    show_override_prompt = [&]() {
        screens.show(create_password_prompt_screen(
            "Override Mode",
            "Enter override password to continue offline",
            "Join",
            "Back",
            [&](const std::string& pass) { // on_join
                if (pass == "9999")
                {
                    show_login();
                }
                else
                {
                    // Re-show con campo limpio (simple retry)
                    show_override_prompt();
                }
            },
            [&]() { // on_cancel
                show_wifi_setup();
            }
        ));
    };

    // Wi-Fi setup flow (first screen) - CON CALLBACK PARA TRANSICIÓN
    show_wifi_setup = [&]() {
        screens.show(create_wifi_settings_panel(
            [&]() { // on_back
                app.quit();
            },
            [&]() { // on_scan_wifi
                show_wifi_setup(); // refresh by recreating
            },
            [&](const std::string& ssid, const std::string& password) { // on_connect
                // Simulated connection result: success if both non-empty
                if (!ssid.empty() && !password.empty())
                {
                    show_login();
                }
                else
                {
                    show_override_prompt();
                }
            },
            [&](const egt_wifi::WiFiNetwork& net) { // on_item_selected
                // Could prefill SSID or display details; for now no-op
                (void)net;
            },
            [&](std::shared_ptr<egt::Widget> screen) { // on_show_screen - CALLBACK PARA TRANSICIÓN
                screens.show(screen);
            }
        ));
    };

    // Start with Wi-Fi setup
    show_wifi_setup();

    win.show();
    app.run();
}