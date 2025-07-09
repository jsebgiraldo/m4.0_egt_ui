#include <egt/ui>
#include <egt/virtualkeyboard.h>
#include "screen_wifi_settings.h"
#include "../app.h"

using namespace egt;
using namespace std;

std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect)
{
    const int width = 800;
    const int height = 480;

    auto root = std::make_shared<Window>(Rect(0, 0, width, height));
    root->color(Palette::ColorId::bg, Palette::white);
    root->show();

    // Título centrado
    auto title = make_shared<Label>("Establish Wi-Fi Connection", Rect(0, 30, width, 40));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(24, Font::Weight::bold));
    root->add(title);

    // Frame exterior
    auto outer_frame = std::make_shared<Window>(Rect(100, 90, 600, 300));
    outer_frame->color(Palette::ColorId::bg, Palette::lightblue);
    root->add(outer_frame);
    outer_frame->show();

    // Frame interior
    auto inner_frame = std::make_shared<Window>(Rect(30, 30, 540, 200));
    inner_frame->color(Palette::ColorId::bg, Palette::white);
    outer_frame->add(inner_frame);
    inner_frame->show();

    auto choose_label = make_shared<Label>("Choose network...", Rect(20, 0, 500, 20));
    choose_label->align(AlignFlag::center_horizontal);
    choose_label->margin(4); // margen de 4
    inner_frame->add(choose_label);

    // ListBox de redes Wi-Fi
    auto list = make_shared<ListBox>(Rect(20, 30, 500, 160));
    list->margin(4); // margen de 4
    auto item1 = make_shared<StringItem>("MyNetwork");
    auto item2 = make_shared<StringItem>("Guest");
    auto item3 = make_shared<StringItem>("IoT_Hub");
    auto item4 = make_shared<StringItem>("OfficeNet");
    list->add_item(item1);
    list->add_item(item2);
    list->add_item(item3);
    list->add_item(item4);

    // Evento al seleccionar una red
    list->on_selected_changed([=]()
    {
        auto selected_item = list->selected();
        if (selected_item) {
			// Crear overlay
            auto overlay = std::make_shared<egt::Window>(Rect(0, 0, width, height));
			overlay->color(Palette::ColorId::bg, egt::Palette::white);
            root->add(overlay);
			overlay->show();
            overlay->zorder_top();

			// Título
            auto title = std::make_shared<Label>("Enter Password", Rect(0, 20, 600, 30));
            title->align(AlignFlag::center_horizontal);
            title->font(Font(22, Font::Weight::bold));
            overlay->add(title);

            // Campo de texto
            auto textbox = std::make_shared<TextBox>("", Rect(20, 70, 550, 60));
            textbox->text_align(AlignFlag::left);
            overlay->add(textbox);

            // Botón Cancel
            auto btn_cancel = std::make_shared<Button>("Cancel", Rect(600, 70, 70, 40));
            btn_cancel->on_click([=](Event&) {
                overlay->hide();
    			overlay->detach();
            });
            overlay->add(btn_cancel);

            // Botón Join
            auto btn_join = std::make_shared<Button>("Join", Rect(710, 70, 70, 40));
            btn_join->color(Palette::ColorId::button_bg, Palette::blue);
            btn_join->on_click([=](Event&) {
                overlay->hide();
    			overlay->detach();
            });
            overlay->add(btn_join);

            // Teclado virtual
            auto vkeyboard = std::make_shared<VirtualKeyboard>(Rect(20, 150, 760, 300));
            overlay->add(vkeyboard);
        }
    });

    inner_frame->add(list);

    // Botón Back fuera del frame
    auto btn_back = make_shared<Button>("Back", Rect(width/2 - 50, 410, 100, 40));
    btn_back->on_click([=](Event&) { on_back(); });
    root->add(btn_back);

    return root;
}