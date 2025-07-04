#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "screens/screen_welcome.h"
#include "screens/screen_menu.h"
#include "screens/screen_wifi_settings.h"
#include "screens/screen_info.h"
#include "screens/screen_monitor.h"

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    std::function<void()> show_menu;

    auto show_monitor = [&]() {
        screens.show(create_monitor_screen(show_menu));
    };

    auto show_settings = [&]() {
        screens.show(create_wifi_settings_panel(
            show_menu, // on_back
            [](){ /* on_save: acción al guardar */ },
            [](const std::string&, const std::string&) { /* on_change: acción al cambiar */ }
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