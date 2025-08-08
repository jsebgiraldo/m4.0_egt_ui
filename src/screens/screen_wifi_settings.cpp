#include <egt/ui>
#include <egt/widget.h>

#include "screen_wifi_settings.h"
#include "screen_password_prompt.h"
#include "../app.h"

#include <map>

using namespace egt;
using namespace egt_wifi;
using namespace std;

std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect,
    function<void(const egt_wifi::WiFiNetwork&)> on_item_selected,
    function<void(std::shared_ptr<egt::Widget>)> on_show_screen)
{
    egt_wifi::WiFiManager wifi;
    auto network_map = make_shared<map<string, egt_wifi::WiFiNetwork>>();

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
    listbox->margin(spacing);

    // Escanear y poblar el ComboBox
    auto networks = wifi.scan_networks();
    for (const auto& net : networks)
    {
        std::string label = net.ssid + " (" + std::to_string(net.signal) + "%)";
        if (net.connected)
            label += " [✔]";
        auto item = std::make_shared<StringItem>(label);
        listbox->add_item(item);
        (*network_map)[label] = net;
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
    (*network_map)[demo_label] = demo_net;

    auto item = make_shared<StringItem>("Other...");
    listbox->add_item(item);

    // Configurar tamaño de items
    auto content = listbox->content_area();  
    int N = static_cast<int>(listbox->item_count());
    Size item_sz{
        content.width(),
        (content.height() - spacing * (N - 1)) / N
    };

    for (int i = 0; i < N; ++i)
    {
        auto w = listbox->item_at(i);
        w->resize(item_sz);
        w->margin(spacing);
    }

    listbox->damage();
    
    // Evento de selección - USANDO TRANSICIÓN COMPLETA
    listbox->on_selected_changed([=]() {
        int selected_index = listbox->selected();
        if (selected_index >= 0 && selected_index < listbox->item_count())
        {
            auto string_item = listbox->item_at(selected_index);
            if (string_item)
            {
                std::string label = string_item->text();
                auto it = network_map->find(label);
                if (it != network_map->end()) {
                    auto selected_net = it->second;
                    
                    auto pwd_screen = create_password_prompt_screen(
                        "Network: " + selected_net.ssid,
                        "Enter password to join",
                        "Join",
                        "Back",
                        [=](const std::string& password) {
                            on_connect(selected_net.ssid, password);
                        },
                        [=]() {
                            // REGRESAR CON TRANSICIÓN COMPLETA
                            if (on_show_screen) {
                                auto wifi_screen = create_wifi_settings_panel(
                                    on_back, 
                                    on_scan_wifi, 
                                    on_connect, 
                                    on_item_selected, 
                                    on_show_screen
                                );
                                on_show_screen(wifi_screen);
                            }
                        }
                    );

                    // USAR TRANSICIÓN EN LUGAR DE OVERLAY
                    if (on_show_screen) {
                        on_show_screen(pwd_screen);
                    }
                }
            }
        }
    });

    outer_vsizer->add(listbox);

    auto btn_back = make_shared<Button>("Back", Rect(0, 0, 100, 40));
    btn_back->align(AlignFlag::bottom | AlignFlag::expand_horizontal);
    btn_back->on_click([=](Event&) {
        on_back();
    });
    vsizer->add(btn_back);

    return root;
}