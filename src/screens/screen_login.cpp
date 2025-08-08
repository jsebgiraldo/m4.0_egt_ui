#include <egt/ui>
#include "screen_login.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_login_screen(function<void()> on_success, function<void()> on_cancel)
{
    const int width = 800;
    const int height = 480;
    const string correct_pin = "1234";
    
    auto container = make_shared<Frame>(Rect(0, 0, width, height));
    container->color(Palette::ColorId::bg, Palette::white);

    auto entered_pin = make_shared<string>();
    
    auto title = make_shared<Label>("Enter Password", Rect(0, 30, width, 50));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(28, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, Palette::blue);
    container->add(title);

    // Display del PIN (asteriscos)
    auto pin_display = make_shared<Label>("", Rect(0, 100, width, 40));
    pin_display->align(AlignFlag::center_horizontal);
    pin_display->font(Font(24, Font::Weight::bold));
    pin_display->color(Palette::ColorId::label_text, Palette::black);
    container->add(pin_display);

    // Función para actualizar el display
    auto update_display = [pin_display, entered_pin]() {
        string asterisks(entered_pin->length(), '*');
        pin_display->text(asterisks);
    };

    // Grid para el teclado numérico (4x3)
    auto keypad_grid = make_shared<SelectableGrid>(StaticGrid::GridSize(3, 4));
    keypad_grid->resize(Size(240, 200));
    keypad_grid->move(Point((width - 240) / 2, 180));
    keypad_grid->horizontal_space(10);
    keypad_grid->vertical_space(10);

    // Crear botones numéricos 1-9
    vector<shared_ptr<Button>> number_buttons;
    for (int i = 1; i <= 9; ++i) {
        auto btn = make_shared<Button>(to_string(i));
        btn->font(Font(20, Font::Weight::bold));
        btn->color(Palette::ColorId::button_bg, Palette::lightgray);
        btn->color(Palette::ColorId::button_fg, Palette::black);
        
        // Capturar el valor del número en el lambda
        btn->on_click([=](Event&) {
            if (entered_pin->length() < 6) { // Limitar a 6 dígitos
                *entered_pin += to_string(i);
                update_display();
            }
        });
        
        number_buttons.push_back(btn);
        
        // Posicionar en grid: (i-1) % 3, (i-1) / 3
        int col = (i - 1) % 3;
        int row = (i - 1) / 3;
        keypad_grid->add(expand(btn), StaticGrid::GridPoint(col, row));
    }

    // Botón "C" (Clear)
    auto btn_clear = make_shared<Button>("C");
    btn_clear->font(Font(20, Font::Weight::bold));
    btn_clear->color(Palette::ColorId::button_bg, Palette::orange);
    btn_clear->color(Palette::ColorId::button_fg, Palette::white);
    btn_clear->on_click([=](Event&) {
        entered_pin->clear();
        update_display();
    });
    keypad_grid->add(expand(btn_clear), StaticGrid::GridPoint(0, 3));

    // Botón "0"
    auto btn_zero = make_shared<Button>("0");
    btn_zero->font(Font(20, Font::Weight::bold));
    btn_zero->color(Palette::ColorId::button_bg, Palette::lightgray);
    btn_zero->color(Palette::ColorId::button_fg, Palette::black);
    btn_zero->on_click([=](Event&) {
        if (entered_pin->length() < 6) {
            *entered_pin += "0";
            update_display();
        }
    });
    keypad_grid->add(expand(btn_zero), StaticGrid::GridPoint(1, 3));

    // Botón "OK"
    auto btn_ok = make_shared<Button>("OK");
    btn_ok->font(Font(20, Font::Weight::bold));
    btn_ok->color(Palette::ColorId::button_bg, Palette::green);
    btn_ok->color(Palette::ColorId::button_fg, Palette::white);
    btn_ok->on_click([=](Event&) {
        if (*entered_pin == correct_pin) {
            on_success();
        } else {
            // PIN incorrecto - limpiar y mostrar mensaje temporal
            entered_pin->clear();
            pin_display->text("Incorrect PIN");
            pin_display->color(Palette::ColorId::label_text, Palette::red);
            
            // Restaurar después de 1 segundo
            auto timer = make_shared<PeriodicTimer>(std::chrono::seconds(1));
            timer->on_timeout([=]() {
                pin_display->text("");
                pin_display->color(Palette::ColorId::label_text, Palette::black);
                timer->cancel();
            });
            timer->start();
        }
    });
    keypad_grid->add(expand(btn_ok), StaticGrid::GridPoint(2, 3));

    container->add(keypad_grid);

    // Botón Cancelar
    auto btn_cancel = make_shared<Button>("Cancel", Rect(width/2 - 60, 420, 120, 40));
    btn_cancel->font(Font(18, Font::Weight::bold));
    btn_cancel->color(Palette::ColorId::button_bg, Palette::red);
    btn_cancel->color(Palette::ColorId::button_fg, Palette::white);
    btn_cancel->on_click([=](Event&) { on_cancel(); });
    container->add(btn_cancel);

    return container;
}