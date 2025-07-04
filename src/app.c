#include "app.h"
#include <egt/ui>
#include "screen_manager.h"
#include "screen_welcome.h"
#include "screens/screen_menu.h"
#include "screens/screen_wifi_settings.h"

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    auto show_menu = [&]() {
        screens.show(create_menu_screen(
            [&]() { // on_settings
                screens.show(create_settings_screen(show_menu));
            },
            [&]() { // on_exit
                app.quit();
            }
        ));
    };

    screens.show(create_welcome_screen(show_menu));
    win.show();
    app.run();
}