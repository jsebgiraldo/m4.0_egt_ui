#include <egt/ui>
#include <egt/widget.h>

#include "screen_wifi_settings.h"
#include "../app.h"


#include <map>

using namespace egt;
using namespace egt_wifi;

using namespace std;


std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect,
    function<void(const egt_wifi::WiFiNetwork&)> on_item_selected)
{

    egt_wifi::WiFiManager wifi;
    std::map<std::string, egt_wifi::WiFiNetwork> network_map;

    const int width = 800;
    const int height = 480;

    auto root = std::make_shared<Window>(Rect(0, 0, width, height));
    root->color(Palette::ColorId::bg, Palette::white);
    root->show();

    auto vsizer = std::make_shared<BoxSizer>(Orientation::vertical);
    root->add(vsizer);

    // Título centrado
    auto title = make_shared<Label>("Establish Wi-Fi Connection", Rect(0, 0, width, 40));
    title->align(AlignFlag::expand_horizontal);
    title->font(Font(24, Font::Weight::bold));
    vsizer->add(title);


    // Frame exterior
    auto outer_frame = std::make_shared<Window>(Rect(0, 0, 700, 360));
    outer_frame->color(Palette::ColorId::bg, Palette::lightblue);
    outer_frame->show();
    vsizer->add(outer_frame);

    auto outer_vsizer = std::make_shared<BoxSizer>(Orientation::vertical);
    outer_vsizer->align(AlignFlag::center);
    outer_frame->add(outer_vsizer);

    auto choose_label = make_shared<Label>("Choose network...", Rect(0, 0, 500, 40));
    choose_label->align(AlignFlag::center);
    outer_vsizer->add(choose_label);

    const int spacing = 4;

    auto listbox = std::make_shared<ListBox>(Rect(0, 0, 600, 300));
    listbox->align(AlignFlag::center);
    listbox->margin(spacing); // margen de 4

    // Escanear y poblar el ComboBox
    auto networks = wifi.scan_networks(); // ahora sí existe ese método
    for (const auto& net : networks)
    {
        std::string label = net.ssid + " (" + std::to_string(net.signal) + "%)";
        if (net.connected)
            label += " [✔]";
        auto item = std::make_shared<StringItem>(label);
        listbox->add_item(item);
        network_map[label] = net;
    }

    // Demo WiFiNetwork para test
    egt_wifi::WiFiNetwork demo_net;
    demo_net.ssid = "DemoNetwork";
    demo_net.signal = 75;
    demo_net.security = "WPA2";
    demo_net.connected = false;
    std::string demo_label = demo_net.ssid + " (" + std::to_string(demo_net.signal) + "%)";
    auto demo_item = std::make_shared<StringItem>(demo_label);
    listbox->add_item(demo_item);
    network_map[demo_label] = demo_net;

    auto item = make_shared<StringItem>("Other...");
    listbox->add_item(item);

    // 3. Calculamos el área interior (sin scrollbars)
    auto content = listbox->content_area();  
    int N = static_cast<int>(listbox->item_count());

    // 4. Definimos el tamaño que debe tener cada ítem
    //    Le restamos un pequeño margen si queremos separación
    Size item_sz{
        content.width(),
        (content.height() - spacing * (N - 1)) / N
    };

    // 5. Aplicamos ese tamaño a cada widget ítem
    for (int i = 0; i < N; ++i)
    {
        auto w = listbox->item_at(i);
        w->resize(item_sz);
        w->margin(spacing);  // opcional: margen superior para separarlos
    }

    // 6. (Opcional) Forzar un re-layout
    listbox->damage();
    
listbox->on_selected_changed([=]() {
    int selected_index = listbox->selected();
    if (selected_index >= 0 && selected_index < listbox->item_count())
    {
        auto string_item = listbox->item_at(selected_index);
        if (string_item)
        {
            std::string label = string_item->text();
            auto it = network_map.find(label);
            if (it != network_map.end())
            {
                root->hide(); 
                outer_frame->hide();
                on_item_selected(it->second);
            }
        }
    }
});
    outer_vsizer->add(listbox);

    auto btn_back = make_shared<Button>("Back", Rect(0, 0, 100, 40));
    btn_back->align(AlignFlag::bottom | AlignFlag::expand_horizontal);
    btn_back->on_click([root, on_back](Event&) {

        on_back();
    });
    vsizer->add(btn_back);

    return root;
}