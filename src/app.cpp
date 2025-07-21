#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "screens/screen_welcome.h"
#include "screens/screen_menu.h"
#include "screens/screen_wifi_settings.h"
#include "screens/screen_info.h"
#include "screens/screen_monitor.h"
#include "screens/screen_wifi_network_details.h"

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    std::function<void()> show_menu;
    std::function<void()> show_settings;

    auto show_monitor = [&]() {
        screens.show(create_monitor_screen(show_menu));
    };

    show_settings = [&]() {
        screens.show(create_wifi_settings_panel(
            show_menu, // on_back
            [](){ /* on_scan_wifi */ },
            [](const std::string&, const std::string&) { /* on_connect */ },
            [&](const egt_wifi::WiFiNetwork& network) { // on_item_selected
                screens.show(create_wifi_network_details_screen(
                    network,
                    show_settings,
                    [](const std::string&, const std::string&) { /* on_connect */ }
                ));
            }
        ));
    };

    auto show_info = [&]() {
        screens.show(create_info_screen(show_menu));
    };

    show_menu = [&]() {
        screens.show(create_menu_screen(
            show_monitor,
            show_settings,
            show_info,
            [&]() { app.quit(); }
        ));
    };

    screens.show(create_welcome_screen(show_menu));
    win.show();
    app.run();
}