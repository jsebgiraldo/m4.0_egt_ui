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

void run_app(int argc, char** argv)
{
    egt::Application app(argc, argv);
    egt::TopWindow win;
    ScreenManager screens(win);

    std::function<void()> show_settings;
    std::function<void()> show_login;
    std::function<void()> show_mode_select;


    // Ahora define show_login
    show_login = [&]() {
        screens.show(create_login_screen(
            [&]() { // on_success
                show_mode_select(); // Ahora ya está declarado
            },
            [&]() { // on_cancel  
                app.quit();
            }
        ));
    };

    show_mode_select = [&]() {
        screens.show(create_mode_select_screen(
            [&]() { // on_training
                screens.show(create_training_screen(
                    300, // 5 minutos de duración
                    [&]() { // on_complete
                        // Mostrar pantalla de resultados o volver al menú
                        show_mode_select();
                    },
                    [&]() { // on_pause
                        // Lógica adicional de pausa si es necesaria
                    },
                    [&]() { // on_cancel
                        show_mode_select(); // Volver a selección de modo
                    }
                ));
            },
            [&]() { // on_treatment
                // Implementar pantalla de tratamiento más adelante
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

    screens.show(create_start_screen([&]() {
        show_login(); 
    }));

    win.show();
    app.run();
}