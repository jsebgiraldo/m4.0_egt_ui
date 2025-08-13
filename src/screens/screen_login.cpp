#include <egt/ui>
#include "screen_login.h"
#include "screen_password_prompt.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_login_screen(
    function<void()> on_success,
    function<void()> on_cancel,
    function<void(const std::string&)> on_select_user)
{
    const int width = 800;
    const int height = 480;

    // Lista de técnicos con sus contraseñas
    struct Technician {
        string name;
        string password;
    };
    
    vector<Technician> technicians = {
        {"Alice", "1234"},
        {"Bob", "5678"},
        {"Charlie", "9999"},
        {"Dana", "1111"}
    };

    auto selected_user = make_shared<string>(technicians[0].name);

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Encabezado
    auto title = make_shared<Label>("Technician Log-In", Rect(0, 30, width, 60));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(32, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Instrucciones
    auto instructions = make_shared<Label>("Select technician to continue", Rect(0, 100, width, 30));
    instructions->align(AlignFlag::center_horizontal);
    instructions->font(Font(18));
    instructions->color(Palette::ColorId::label_text, Palette::gray);
    container->add(instructions);

    // ListBox de técnicos - más grande y centrado
    auto user_list = make_shared<ListBox>(Rect(width/2 - 200, 140, 400, 200));
    for (const auto& tech : technicians) {
        auto item = make_shared<StringItem>(tech.name);
        item->font(Font(20)); // Fuente más grande
        user_list->add_item(item);
    }
    user_list->selected(0);
    container->add(user_list);

    // Callback para cuando se selecciona un técnico
    user_list->on_selected_changed([=]() {
        int idx = user_list->selected();
        if (idx >= 0 && idx < (int)technicians.size()) {
            *selected_user = technicians[idx].name;
            if (on_select_user) on_select_user(*selected_user);
        }
    });

    // Función para obtener la contraseña del técnico seleccionado
    auto get_selected_password = [=]() -> string {
        for (const auto& tech : technicians) {
            if (tech.name == *selected_user) {
                return tech.password;
            }
        }
        return "";
    };

    // Botones en layout horizontal
    auto button_container = make_shared<Frame>(Rect(0, 360, width, 60));
    button_container->color(Palette::ColorId::bg, Palette::white);
    container->add(button_container);

    // Botón Cancel - lado izquierdo
    auto btn_cancel = make_shared<Button>("Cancel", Rect(100, 10, 120, 40));
    btn_cancel->font(Font(18, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::lightgray);
    btn_cancel->color(Palette::ColorId::button_fg, Palette::black);
    btn_cancel->on_click([=](Event&) { 
        on_cancel(); 
    });
    button_container->add(btn_cancel);

    // Botón Login - lado derecho
    auto btn_login = make_shared<Button>("Login", Rect(580, 10, 120, 40));
    btn_login->font(Font(18, Font::Weight::bold));
    btn_login->color(Palette::ColorId::button_bg, Palette::green);
    btn_login->color(Palette::ColorId::button_fg, Palette::white);
    button_container->add(btn_login);

    // Variable para almacenar la pantalla de contraseña
    auto password_screen = make_shared<shared_ptr<Widget>>(nullptr);

    btn_login->on_click([=](Event&) {
        string selected_name = *selected_user;
        string correct_password = get_selected_password();
        
        // Crear pantalla de contraseña
        *password_screen = create_password_prompt_screen(
            "Login: " + selected_name,
            "Enter your password",
            "Login",
            "Back",
            [=](const std::string& entered_password) { // on_join
                if (entered_password == correct_password) {
                    // Contraseña correcta - proceder al éxito
                    on_success();
                } else {
                    // Contraseña incorrecta - mostrar error y volver a la pantalla de login
                    printf("Incorrect password for %s\n", selected_name.c_str());
                }
            },
            [=]() { // on_cancel
                // Volver a la pantalla de login (no hacer nada, se cierra automáticamente)
                printf("Password entry cancelled\n");
            }
        );
    });

    return container;
}

// Nueva función que acepta callback para cambio de pantalla
shared_ptr<Widget> create_login_screen_with_navigation(
    function<void()> on_success,
    function<void()> on_cancel,
    function<void(const std::string&)> on_select_user,
    function<void(shared_ptr<Widget>)> on_show_screen)
{
    const int width = 800;
    const int height = 480;

    // Lista de técnicos con sus contraseñas
    struct Technician {
        string name;
        string password;
    };
    
    vector<Technician> technicians = {
        {"Alice", "1234"},
        {"Bob", "5678"}, 
        {"Charlie", "9999"},
        {"Dana", "1111"}
    };

    auto selected_user = make_shared<string>(technicians[0].name);

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Encabezado
    auto title = make_shared<Label>("Technician Log-In", Rect(0, 30, width, 60));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(32, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Instrucciones
    auto instructions = make_shared<Label>("Select technician to continue", Rect(0, 100, width, 30));
    instructions->align(AlignFlag::center_horizontal);
    instructions->font(Font(18));
    instructions->color(Palette::ColorId::label_text, Palette::gray);
    container->add(instructions);

    // ListBox de técnicos - más grande y centrado
    auto user_list = make_shared<ListBox>(Rect(width/2 - 200, 140, 400, 200));
    for (const auto& tech : technicians) {
        auto item = make_shared<StringItem>(tech.name);
        item->font(Font(20)); // Fuente más grande
        user_list->add_item(item);
    }
    user_list->selected(0);
    container->add(user_list);

    // Callback para cuando se selecciona un técnico
    user_list->on_selected_changed([=]() {
        int idx = user_list->selected();
        if (idx >= 0 && idx < (int)technicians.size()) {
            *selected_user = technicians[idx].name;
            if (on_select_user) on_select_user(*selected_user);
        }
    });

    // Función para obtener la contraseña del técnico seleccionado
    auto get_selected_password = [=]() -> string {
        for (const auto& tech : technicians) {
            if (tech.name == *selected_user) {
                return tech.password;
            }
        }
        return "";
    };

    // Botones en layout horizontal
    auto button_container = make_shared<Frame>(Rect(0, 360, width, 60));
    button_container->color(Palette::ColorId::bg, Palette::white);
    container->add(button_container);

    // Botón Cancel - lado izquierdo
    auto btn_cancel = make_shared<Button>("Back", Rect(100, 10, 120, 40));
    btn_cancel->font(Font(18, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::lightgray);
    btn_cancel->color(Palette::ColorId::button_fg, Palette::black);
    btn_cancel->on_click([=](Event&) { 
        on_cancel(); 
    });
    button_container->add(btn_cancel);

    // Botón Login - lado derecho
    auto btn_login = make_shared<Button>("Login", Rect(580, 10, 120, 40));
    btn_login->font(Font(18, Font::Weight::bold));
    btn_login->color(Palette::ColorId::button_bg, Palette::green);
    btn_login->color(Palette::ColorId::button_fg, Palette::white);
    button_container->add(btn_login);

    btn_login->on_click([=](Event&) {
        string selected_name = *selected_user;
        string correct_password = get_selected_password();
        
        // Crear pantalla de contraseña
        auto password_screen = create_password_prompt_screen(
            "Login: " + selected_name,
            "Enter your password",
            "Login",
            "Back",
            [=](const std::string& entered_password) { // on_join
                if (entered_password == correct_password) {
                    // Contraseña correcta - proceder al éxito
                    on_success();
                } else {
                    // Contraseña incorrecta - volver a la pantalla de login
                    if (on_show_screen) {
                        auto login_screen = create_login_screen_with_navigation(
                            on_success, on_cancel, on_select_user, on_show_screen
                        );
                        on_show_screen(login_screen);
                    }
                }
            },
            [=]() { // on_cancel - volver a la pantalla de login
                if (on_show_screen) {
                    auto login_screen = create_login_screen_with_navigation(
                        on_success, on_cancel, on_select_user, on_show_screen
                    );
                    on_show_screen(login_screen);
                }
            }
        );
        
        // Cambiar a la pantalla de contraseña
        if (on_show_screen) {
            on_show_screen(password_screen);
        }
    });

    return container;
}