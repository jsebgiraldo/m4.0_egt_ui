#include <egt/ui>
#include "screen_mode_select.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_mode_select_screen(
    function<void()> on_training,
    function<void()> on_treatment, 
    function<void()> on_back)
{
    const int width = 800;
    const int height = 480;
    
    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    // Título centrado
    auto title = make_shared<Label>("Select Mode", Rect(0, 40, width, 60));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(32, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Botón "Solo entrenamiento"
    auto btn_training = make_shared<Button>("Training Only", Rect(width/2 - 180, 140, 360, 80));
    btn_training->align(AlignFlag::center_horizontal);
    btn_training->font(Font(24, Font::Weight::bold));
    btn_training->color(Palette::ColorId::button_bg, Palette::green);
    btn_training->color(Palette::ColorId::button_fg, Palette::white);
    btn_training->margin(10);
    btn_training->on_click([=](Event&) { 
        on_training(); 
    });
    container->add(btn_training);

    // Botón "Iniciar tratamiento"
    auto btn_treatment = make_shared<Button>("Start Treatment", Rect(width/2 - 180, 250, 360, 80));
    btn_treatment->align(AlignFlag::center_horizontal);
    btn_treatment->font(Font(24, Font::Weight::bold));
    btn_treatment->color(Palette::ColorId::button_bg, Palette::blue);
    btn_treatment->color(Palette::ColorId::button_fg, Palette::white);
    btn_treatment->margin(10);
    btn_treatment->on_click([=](Event&) { 
        on_treatment(); 
    });
    container->add(btn_treatment);

    // Botón "Volver"
    auto btn_back = make_shared<Button>("Back", Rect(width/2 - 80, 380, 160, 50));
    btn_back->align(AlignFlag::center_horizontal);
    btn_back->font(Font(20, Font::Weight::bold));
    btn_back->color(Palette::ColorId::button_bg, Palette::lightgray);
    btn_back->color(Palette::ColorId::button_fg, Palette::black);
    btn_back->margin(8);
    btn_back->on_click([=](Event&) { 
        on_back(); 
    });
    container->add(btn_back);

    return container;
}