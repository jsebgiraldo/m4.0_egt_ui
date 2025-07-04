#include <egt/ui>
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

    auto root = make_shared<Frame>(Rect(0, 0, width, height));

    // Título centrado
    auto title = make_shared<Label>("Establish Wi-Fi Connection", Rect(0, 30, width, 40));
    title->align(AlignFlag::center_horizontal);
    title->font(Font(24, Font::Weight::bold));
    root->add(title);

    // Frame exterior
    auto outer_frame = make_shared<Frame>(Rect(100, 90, 600, 300));
    outer_frame->color(Palette::ColorId::bg, Palette::lightblue);
    root->add(outer_frame);

    // Frame interior
    auto inner_frame = make_shared<Frame>(Rect(30, 30, 540, 200));
    inner_frame->color(Palette::ColorId::bg, Palette::white);
    outer_frame->add(inner_frame);

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
    list->on_selected_changed([list]()
    {
        auto selected_item = list->selected();
        if (selected_item)
        {
            fflush(stdout);
        }
    });

    inner_frame->add(list);

    // Botón Back fuera del frame
    auto btn_back = make_shared<Button>("Back", Rect(width/2 - 50, 410, 100, 40));
    btn_back->on_click([=](Event&) { on_back(); });
    root->add(btn_back);

    return root;
}