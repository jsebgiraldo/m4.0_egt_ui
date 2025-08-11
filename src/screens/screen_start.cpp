#include <egt/ui>
#include "screen_start.h"
#include <chrono>

using namespace egt;
using namespace std;

shared_ptr<Widget> create_start_screen(function<void()> on_next)
{
    const int width = 800;
    const int height = 480;

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Título principal
    auto title = make_shared<Label>("Lice Clinics", Rect(0, 80, width, 80));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(48, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Subtítulo con estado de conexión
    auto subtitle = make_shared<Label>("Connecting Wi-Fi...", Rect(0, 180, width, 40));
    subtitle->align(AlignFlag::center_horizontal);
    subtitle->font(Font(24));
    subtitle->color(Palette::ColorId::label_text, Palette::gray);
    container->add(subtitle);

    // Barra de progreso circular (usando ProgressBar en modo indeterminado)
    auto progress = make_shared<ProgressBar>(Rect(width/2 - 100, 240, 200, 20));
    progress->align(AlignFlag::center_horizontal);
    progress->color(Palette::ColorId::button_bg, Palette::blue);
    progress->show_label(false);
    container->add(progress);

    // Botón "Start" (inicialmente oculto)
    auto btn_start = make_shared<Button>("Start", Rect(width/2 - 120, 280, 240, 60));
    btn_start->align(AlignFlag::center_horizontal);
    btn_start->font(Font(24, Font::Weight::bold));
    btn_start->color(Palette::ColorId::button_bg, Palette::blue);
    btn_start->color(Palette::ColorId::button_fg, Palette::white);
    btn_start->margin(10);
    btn_start->hide(); // Inicialmente oculto
    btn_start->on_click([=](Event&) { 
        on_next(); 
    });
    container->add(btn_start);

    // Estado de progreso
    auto progress_value = make_shared<int>(0);
    auto connection_complete = make_shared<bool>(false);

    // Timer para simular progreso de conexión
    auto progress_timer = make_shared<PeriodicTimer>(chrono::milliseconds(100));
    progress_timer->on_timeout([=]() {
        if (!*connection_complete) {
            (*progress_value) += 2;
            progress->value(*progress_value);
            
            // Simular conexión completada al 100%
            if (*progress_value >= 100) {
                *connection_complete = true;
                progress_timer->cancel();
                
                // Actualizar UI cuando la conexión esté completa
                subtitle->text("Wi-Fi Connected");
                subtitle->color(Palette::ColorId::label_text, Palette::green);
                progress->hide();
                btn_start->show();
                
                // Opcional: agregar un breve delay antes de mostrar el botón
                auto delay_timer = make_shared<PeriodicTimer>(chrono::milliseconds(500));
                delay_timer->on_timeout([=]() {
                    delay_timer->cancel();
                });
                delay_timer->start();
            }
        }
    });

    // Callback para conexión exitosa (llamar desde el exterior)
    auto on_connected = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            progress_timer->cancel();
            subtitle->text("Wi-Fi Connected");
            subtitle->color(Palette::ColorId::label_text, Palette::green);
            progress->hide();
            btn_start->show();
        }
    };

    // Callback para error de conexión
    auto on_connection_failed = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            progress_timer->cancel();
            subtitle->text("Connection Failed - Tap to Continue");
            subtitle->color(Palette::ColorId::label_text, Palette::red);
            progress->hide();
            btn_start->text("Continue Offline");
            btn_start->show();
        }
    };

    // Iniciar simulación de progreso
    progress_timer->start();

    // Opcional: Timeout después de 10 segundos para mostrar error
    auto timeout_timer = make_shared<PeriodicTimer>(chrono::seconds(10));
    timeout_timer->on_timeout([=]() {
        if (!*connection_complete) {
            on_connection_failed();
        }
        timeout_timer->cancel();
    });
    timeout_timer->start();

    return container;
}

shared_ptr<Widget> create_start_screen_with_wifi(
    function<void()> on_next,
    function<void()> on_connection_complete,
    function<void()> on_connection_failed)
{
    const int width = 800;
    const int height = 480;

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Título principal
    auto title = make_shared<Label>("Lice Clinics", Rect(0, 80, width, 80));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(48, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Subtítulo con estado de conexión - INICIALMENTE VACÍO
    auto subtitle = make_shared<Label>("Checking Wi-Fi status...", Rect(0, 180, width, 40));
    subtitle->align(AlignFlag::center_horizontal);
    subtitle->font(Font(24));
    subtitle->color(Palette::ColorId::label_text, Palette::gray);
    container->add(subtitle);

    // Spinner circular usando ProgressBar
    auto spinner = make_shared<ProgressBar>(Rect(width/2 - 30, 240, 60, 20));
    spinner->align(AlignFlag::center_horizontal);
    spinner->color(Palette::ColorId::button_bg, Palette::blue);
    spinner->show_label(false);
    container->add(spinner);

    // Botón "Start" (inicialmente oculto)
    auto btn_start = make_shared<Button>("Start", Rect(width/2 - 120, 320, 240, 60));
    btn_start->align(AlignFlag::center_horizontal);
    btn_start->font(Font(24, Font::Weight::bold));
    btn_start->color(Palette::ColorId::button_bg, Palette::blue);
    btn_start->color(Palette::ColorId::button_fg, Palette::white);
    btn_start->margin(10);
    btn_start->hide(); // Inicialmente oculto
    btn_start->on_click([=](Event&) { 
        on_next(); 
    });
    container->add(btn_start);

    // Estado global
    auto connection_complete = make_shared<bool>(false);

    // Función para verificar estado actual de Wi-Fi
    auto check_wifi_status = [=]() -> bool {
        // Opción 1: Usar iwconfig
        //int result = system("iwconfig 2>/dev/null | grep -q 'ESSID:\"'");
        //return result == 0;
        
        // Opción 2: Usar NetworkManager
        int result = system("nmcli -t -f WIFI,STATE g | grep -q 'enabled:connected'");
        return 0;
        
        // Opción 3: Verificar interfaz específica
        // int result = system("cat /sys/class/net/wlan0/operstate | grep -q 'up'");
        // return result == 0;
    };

    // Animación del spinner
    auto spinner_value = make_shared<int>(0);
    auto spinner_timer = make_shared<PeriodicTimer>(chrono::milliseconds(50));
    spinner_timer->on_timeout([=]() {
        if (!*connection_complete) {
            (*spinner_value) += 5;
            if (*spinner_value > 100) *spinner_value = 0;
            spinner->value(*spinner_value);
        }
    });

    // Funciones de callback internas
    auto handle_already_connected = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            spinner_timer->cancel();
            subtitle->text("Wi-Fi Already Connected");
            subtitle->color(Palette::ColorId::label_text, Palette::green);
            spinner->hide();
            btn_start->show();
            if (on_connection_complete) on_connection_complete();
        }
    };

    auto handle_needs_setup = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            spinner_timer->cancel();
            subtitle->text("Wi-Fi Setup Required");
            subtitle->color(Palette::ColorId::label_text, Palette::orange);
            spinner->hide();
            btn_start->text("Setup Wi-Fi");
            btn_start->show();
            // NO llamar on_connection_complete aquí - necesita configuración
        }
    };

    auto handle_connection_success = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            spinner_timer->cancel();
            subtitle->text("Wi-Fi Connected");
            subtitle->color(Palette::ColorId::label_text, Palette::green);
            spinner->hide();
            btn_start->show();
            if (on_connection_complete) on_connection_complete();
        }
    };

    auto handle_failure = [=]() {
        if (!*connection_complete) {
            *connection_complete = true;
            spinner_timer->cancel();
            subtitle->text("Connection Failed - Continue Offline");
            subtitle->color(Palette::ColorId::label_text, Palette::red);
            spinner->hide();
            btn_start->text("Continue Offline");
            btn_start->show();
            if (on_connection_failed) on_connection_failed();
        }
    };

    // Verificación inicial del estado Wi-Fi después de 1 segundo
    auto initial_check_timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    initial_check_timer->on_timeout([=]() {
        initial_check_timer->cancel();
        
        bool already_connected = check_wifi_status();
        
        if (already_connected) {
            // YA CONECTADO - mostrar Start directamente
            printf("Wi-Fi already connected.\n");
            handle_already_connected();
        } else {
            // NO CONECTADO - mostrar progreso de conexión
            subtitle->text("Connecting Wi-Fi...");
            spinner_timer->start();
            
            // Timeout automático después de 6 segundos si no se conecta
            auto timeout_timer = make_shared<PeriodicTimer>(chrono::seconds(6));
            timeout_timer->on_timeout([=]() {
                timeout_timer->cancel();
                handle_needs_setup(); // Cambiar a "Setup Required" en lugar de fallo
            });
            timeout_timer->start();

            // Simulación de éxito después de 3 segundos (para demo)
            auto success_timer = make_shared<PeriodicTimer>(chrono::seconds(3));
            success_timer->on_timeout([=]() {
                success_timer->cancel();
                //handle_connection_success();
                handle_failure();
            });
            success_timer->start();
        }
    });
    initial_check_timer->start();

    return container;
}