#include <egt/ui>
#include "screen_start.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_start_screen(function<void()> on_next)
{
    const int width = 800;
    const int height = 480;

    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Título centrado "M4.0 UI"
    auto title = make_shared<Label>("M4.0 UI", Rect(0, 120, width, 80));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(48, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Botón "Empezar" grande y centrado
    auto btn_start = make_shared<Button>("Start", Rect(width/2 - 120, 280, 240, 60));
    btn_start->align(AlignFlag::center_horizontal);
    btn_start->font(Font(24, Font::Weight::bold));
    btn_start->color(Palette::ColorId::button_bg, Palette::blue);
    btn_start->color(Palette::ColorId::button_fg, Palette::white);
    btn_start->margin(10);
    
    // Evento del botón
    btn_start->on_click([=](Event&) { 
        on_next(); 
    });
    
    container->add(btn_start);

    return container;
}