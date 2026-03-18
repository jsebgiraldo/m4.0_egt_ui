#include <egt/ui>
#include <egt/widget.h>

#include "screen_wifi_settings.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

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

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (top-left) ─────────────────────────────────────────────────────
    auto logo = ui::create_logo(10, 10, 166, 103);
    container->add(logo);

    // ── Title (Figma: "Establish Wi-Fi Connection", gray) ───────────────────
    auto title = make_shared<Label>("Establish Wi-Fi Connection",
        Rect(180, 30, 400, 30));
    title->align(AlignFlag::left);
    title->font(dt::fontSubtitle());
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── Divider line ────────────────────────────────────────────────────────
    auto divider = make_shared<Frame>(Rect(0, 120, dt::SCREEN_W, 1));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    // ── "Choose a Network..." label ─────────────────────────────────────────
    auto choose_label = make_shared<Label>("Choose a Network...",
        Rect(40, 130, 300, 25));
    choose_label->font(dt::fontBody());
    choose_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(choose_label);

    // ── Network list (Figma shows flat list with dividers) ──────────────────
    auto networks = wifi.scan_networks();

    // Add demo network for testing
    WiFiNetwork demo_net;
    demo_net.ssid = "DemoNetwork";
    demo_net.signal = 75;
    demo_net.security = "WPA2";
    demo_net.connected = false;
    networks.push_back(demo_net);

    const int list_x = 40;
    const int list_y = 160;
    const int item_h = 45;
    const int list_w = dt::SCREEN_W - 80;

    for (size_t i = 0; i < networks.size() && i < 6; i++) {
        const auto& net = networks[i];
        int y = list_y + static_cast<int>(i) * item_h;

        string label = net.ssid;
        (*network_map)[label] = net;

        // Network name label
        Color text_color = net.connected ? dt::kGreen : dt::kTextPrimary;
        string suffix = net.connected ? " [Connected]" : "";

        auto net_btn = make_shared<Button>(label + suffix,
            Rect(list_x, y, list_w, item_h - 5));
        net_btn->font(dt::fontBody());
        net_btn->color(Palette::ColorId::button_bg, dt::kWhite);
        net_btn->color(Palette::ColorId::button_text, text_color);
        net_btn->color(Palette::ColorId::border, dt::kGrayLight);
        net_btn->border(1);
        net_btn->border_radius(dt::RADIUS_SM);

        net_btn->on_click([=](Event&) {
            auto selected_net = (*network_map)[label];
            auto pwd_screen = create_password_prompt_screen(
                "Network: " + selected_net.ssid,
                "Enter password to join",
                "Join", "Back",
                [=](const string& password) {
                    on_connect(selected_net.ssid, password);
                },
                [=]() {
                    if (on_show_screen) {
                        auto wifi_screen = create_wifi_settings_panel(
                            on_back, on_scan_wifi, on_connect,
                            on_item_selected, on_show_screen);
                        on_show_screen(wifi_screen);
                    }
                });
            if (on_show_screen) on_show_screen(pwd_screen);
        });
        container->add(net_btn);
    }

    // ── "Other..." entry ────────────────────────────────────────────────────
    int other_y = list_y + static_cast<int>(min(networks.size(), (size_t)6)) * item_h;
    auto btn_other = make_shared<Button>("Other...",
        Rect(list_x, other_y, list_w, item_h - 5));
    btn_other->font(dt::fontBody());
    btn_other->color(Palette::ColorId::button_bg, dt::kWhite);
    btn_other->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn_other->color(Palette::ColorId::border, dt::kGrayLight);
    btn_other->border(1);
    btn_other->border_radius(dt::RADIUS_SM);
    container->add(btn_other);

    // ── Back button (bottom-left) ───────────────────────────────────────────
    auto btn_back = ui::create_outlined_button("Back",
        Rect(30, dt::SCREEN_H - 80, 156, 61),
        on_back);
    container->add(btn_back);

    return container;
}