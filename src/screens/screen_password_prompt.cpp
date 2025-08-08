#include <egt/ui>
#include <egt/widget.h>
#include <egt/virtualkeyboard.h>
#include "screen_password_prompt.h"

using namespace egt;
using namespace std;

std::shared_ptr<Widget> create_password_prompt_screen(
    const std::string& title_text,
    const std::string& message,
    const std::string& join_label,
    const std::string& cancel_label,
    std::function<void(const std::string& password)> on_join,
    std::function<void()> on_cancel)
{
    // Frame de fondo SÓLIDO que cubre toda la pantalla (sin transparencia)
    auto overlay = make_shared<Frame>(Rect(0, 0, 800, 480));
    overlay->color(Palette::ColorId::bg, Palette::lightgray); // Color sólido
    
    // Contenedor principal centrado
    auto main_container = make_shared<Frame>(Rect(50, 20, 700, 440));
    main_container->color(Palette::ColorId::bg, Palette::white);
    main_container->border(2);
    overlay->add(main_container);

    auto vsizer = make_shared<BoxSizer>(Orientation::vertical);
    vsizer->align(AlignFlag::expand);
    vsizer->margin(15);
    main_container->add(vsizer);

    // Título - más compacto
    auto title = make_shared<Label>(title_text);
    title->font(Font(22, Font::Weight::bold));
    title->align(AlignFlag::center_horizontal);
    title->margin(5);
    vsizer->add(title);

    // Mensaje/instrucciones - más compacto
    auto info = make_shared<Label>(message);
    info->font(Font(14));
    info->align(AlignFlag::center_horizontal);
    info->margin(3);
    vsizer->add(info);

    // Layout horizontal para campo de contraseña y botones
    auto form_layout = make_shared<BoxSizer>(Orientation::horizontal);
    form_layout->align(AlignFlag::expand_horizontal);
    form_layout->margin(10);

    // Campo de contraseña - optimizado para el espacio
    auto pwd = make_shared<TextBox>("");
    pwd->text_align(AlignFlag::left);
    pwd->align(AlignFlag::expand_horizontal);
    pwd->resize(Size(400, 40));
    pwd->font(Font(16));
    pwd->margin(5);
    form_layout->add(pwd);

    // Botón Cancel - más compacto
    auto btn_cancel = make_shared<Button>(cancel_label);
    btn_cancel->font(Font(14, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::lightgray);
    btn_cancel->color(Palette::ColorId::button_fg, Palette::black);
    btn_cancel->resize(Size(80, 40));
    btn_cancel->margin(3);
    btn_cancel->on_click([=](Event&) {
        on_cancel();
    });
    form_layout->add(btn_cancel);

    // Botón Join - más compacto
    auto btn_join = make_shared<Button>(join_label);
    btn_join->font(Font(14, Font::Weight::bold));
    btn_join->color(Palette::ColorId::button_bg, Palette::blue);
    btn_join->color(Palette::ColorId::button_fg, Palette::white);
    btn_join->resize(Size(80, 40));
    btn_join->margin(3);
    btn_join->on_click([=](Event&) {
        on_join(pwd->text());
    });
    form_layout->add(btn_join);

    vsizer->add(form_layout);

    // Teclado virtual - optimizado para 800x480
    auto vkeyboard = make_shared<VirtualKeyboard>();
    vkeyboard->resize(Size(670, 260)); // Reducido para caber mejor
    vkeyboard->align(AlignFlag::center_horizontal);
    vkeyboard->margin(5);
    
    vsizer->add(vkeyboard);

    overlay->damage();
    return overlay;
}