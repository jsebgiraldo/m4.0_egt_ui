#include "screen_wifi_network_details.h"
#include <egt/ui>
#include <egt/widget.h>
#include <egt/virtualkeyboard.h>

#include <string>

using namespace egt;
using namespace egt_wifi;

std::shared_ptr<egt::Widget> create_wifi_network_details_screen(
    const egt_wifi::WiFiNetwork& network,
    std::function<void()> on_back,
    std::function<void(const std::string& ssid, const std::string& password)> on_connect
)
{
    auto root = std::make_shared<Window>(Rect(0, 0, 800, 480));
    root->color(Palette::ColorId::bg, Palette::white);
    
    auto main_sizer = std::make_shared<BoxSizer>(Orientation::vertical);
    main_sizer->align(AlignFlag::expand);
    main_sizer->margin(20);
    root->add(main_sizer);

    // Título
    auto title = std::make_shared<Label>("Network: " + network.ssid);
    title->font(Font(22, Font::Weight::bold));
    title->align(AlignFlag::center_horizontal);
    title->margin(10);
    main_sizer->add(title);

    // Layout para contraseña y botones
    auto form_sizer = std::make_shared<BoxSizer>(Orientation::horizontal);
    form_sizer->align(AlignFlag::expand_horizontal);
    form_sizer->margin(10);

    auto textbox = std::make_shared<TextBox>("Enter password");
    textbox->text_align(AlignFlag::left);
    textbox->align(AlignFlag::expand_horizontal);
    textbox->resize(Size(400, 50));
    textbox->font(Font(16));
    form_sizer->add(textbox);

    auto btn_join = std::make_shared<Button>("Join");
    btn_join->margin(5);
    btn_join->resize(Size(100, 50));
    btn_join->color(Palette::ColorId::button_bg, Color(30, 136, 229));
    btn_join->color(Palette::ColorId::button_fg, Palette::white);
    btn_join->color(Palette::ColorId::label_text, Palette::white);
    btn_join->font(Font(16, Font::Weight::bold));
    btn_join->on_click([=](Event&) {
        on_connect(network.ssid, textbox->text());
    });
    form_sizer->add(btn_join);

    auto btn_cancel = std::make_shared<Button>("Cancel");
    btn_cancel->margin(5);
    btn_cancel->resize(Size(100, 50));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::white);
    btn_cancel->color(Palette::ColorId::button_fg, Color(80, 80, 80));
    btn_cancel->color(Palette::ColorId::label_text, Color(80, 80, 80));
    btn_cancel->color(Palette::ColorId::border, Color(200, 200, 200));
    btn_cancel->border(1);
    btn_cancel->font(Font(16));
    btn_cancel->on_click([on_back](Event&) {
        on_back();
    });
    form_sizer->add(btn_cancel);

    main_sizer->add(form_sizer);

    // Crear teclado virtual con tamaño apropiado
    auto keyboard_frame = std::make_shared<Frame>();
    keyboard_frame->align(AlignFlag::expand);
    keyboard_frame->resize(Size(760, 280));
    
    auto vkeyboard = std::make_shared<VirtualKeyboard>();
    vkeyboard->align(AlignFlag::expand);
    
    // Configurar el teclado virtual para enviar texto al textbox
    vkeyboard->on_event([=](Event& event) {
        if (event.id() == EventId::keyboard_down) {
            auto key = event.key();
            
            if (key.keycode == EKEY_ENTER) {
                // Enter presionado, conectar
                on_connect(network.ssid, textbox->text());
            }
            else if (key.keycode == EKEY_BACKSPACE) {
                // Backspace
                std::string current = textbox->text();
                if (!current.empty()) {
                    current.pop_back();
                    textbox->text(current);
                }
            }
            else if (key.unicode != 0) {
                // Carácter normal
                std::string current = textbox->text();
                if (current == "Enter password") {
                    current = "";
                }
                current += static_cast<char>(key.unicode);
                textbox->text(current);
            }
        }
    });

    keyboard_frame->add(vkeyboard);
    main_sizer->add(keyboard_frame);

    // Hacer que el textbox tenga foco inicial - manejar el warning
    (void)textbox->focus();

    root->show();
    root->damage();

    return root;
}