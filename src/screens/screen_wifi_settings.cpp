#include <egt/ui>
#include <egt/widget.h>

#include "screen_wifi_settings.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <map>
#include <algorithm>
#include <cmath>

using namespace egt;
using namespace egt_wifi;
using namespace std;

// ── WiFi signal-arc icon (3 concentric arcs + dot) ──────────────────────────
class WifiIcon : public Widget {
public:
    WifiIcon(const Rect& rect, const Color& col)
        : Widget(rect), m_color(col)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        auto center = Point(b.x() + b.width() / 2,
                            b.y() + b.height() - 2);

        constexpr float start = -M_PI * 0.75f;   // -135°
        constexpr float end   = -M_PI * 0.25f;   // -45°

        painter.line_width(2.5f);
        painter.set(m_color);

        float radii[] = {7.0f, 13.0f, 19.0f};
        for (float r : radii) {
            painter.draw(Arc(center, r, start, end));
            painter.stroke();
        }

        // small dot at the base
        painter.draw(Arc(center, 2.0f, 0.0f, 2.0f * M_PI));
        painter.fill();
    }

private:
    Color m_color;
};

std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect,
    function<void(const egt_wifi::WiFiNetwork&)> on_item_selected,
    function<void(std::shared_ptr<egt::Widget>)> on_show_screen,
    int scroll_offset,
    std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> cached_networks)
{
    // ── Scan or use cached networks ─────────────────────────────────────────
    auto nets = cached_networks;
    if (!nets) {
        egt_wifi::WiFiManager wifi;
        nets = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());
    }
    const int total = static_cast<int>(nets->size());

    auto network_map = make_shared<map<string, egt_wifi::WiFiNetwork>>();

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Title ───────────────────────────────────────────────────────────────
    auto title = make_shared<Label>("Establish Wi-Fi Connection",
        Rect(0, 20, dt::SCREEN_W, 30));
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── Determine connected state ───────────────────────────────────────────
    bool any_connected = false;
    for (const auto& net : *nets)
        if (net.connected) { any_connected = true; break; }

    // ── Card container ──────────────────────────────────────────────────────
    const int card_x = 40;
    const int card_y = 55;
    const int card_w = dt::SCREEN_W - 80;  // 720
    const int card_h = dt::SCREEN_H - 120; // 360

    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->color(Palette::ColorId::border,
        any_connected ? dt::kGreen : dt::kGrayLight);
    card->border(2);
    card->border_radius(dt::RADIUS_MD);
    container->add(card);

    // ── "Choose a Network..." header ────────────────────────────────────────
    auto choose_label = make_shared<Label>("Choose a Network...",
        Rect(30, 15, 300, 28));
    choose_label->font(Font(22, Font::Weight::normal));
    choose_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(choose_label);

    // Header separator line
    auto hdr_line = make_shared<Frame>(Rect(10, 48, card_w - 20, 1));
    hdr_line->fill_flags({Theme::FillFlag::blend});
    hdr_line->color(Palette::ColorId::bg, dt::kGrayLight);
    hdr_line->border(0);
    card->add(hdr_line);

    // ── Scroll calculation ──────────────────────────────────────────────────
    const int row_start_y = 52;
    const int row_h = 55;
    const int text_pad = 20;
    const int max_rows = 4;  // max network rows visible at once (matches Figma)
    const int visible = min(total - scroll_offset, max_rows);
    const bool can_scroll_up = scroll_offset > 0;
    const bool can_scroll_down = (scroll_offset + max_rows) < total;

    // ── Scroll ▲/▼ buttons (in header area, right side) ────────────────────
    if (can_scroll_up) {
        auto up_btn = make_shared<Button>("▲",
            Rect(card_w - 80, 5, 32, 28));
        up_btn->font(Font(14, Font::Weight::bold));
        up_btn->color(Palette::ColorId::button_bg, dt::kGrayLight);
        up_btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
        up_btn->border(1);
        up_btn->border_radius(6);
        up_btn->on_click([=](Event&) {
            if (on_show_screen) {
                int new_offset = max(0, scroll_offset - max_rows);
                on_show_screen(create_wifi_settings_panel(
                    on_back, on_scan_wifi, on_connect,
                    on_item_selected, on_show_screen,
                    new_offset, nets));
            }
        });
        card->add(up_btn);
    }

    if (can_scroll_down) {
        auto dn_btn = make_shared<Button>("▼",
            Rect(card_w - 42, 5, 32, 28));
        dn_btn->font(Font(14, Font::Weight::bold));
        dn_btn->color(Palette::ColorId::button_bg, dt::kGrayLight);
        dn_btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
        dn_btn->border(1);
        dn_btn->border_radius(6);
        dn_btn->on_click([=](Event&) {
            if (on_show_screen) {
                int new_offset = min(scroll_offset + max_rows, total - 1);
                on_show_screen(create_wifi_settings_panel(
                    on_back, on_scan_wifi, on_connect,
                    on_item_selected, on_show_screen,
                    new_offset, nets));
            }
        });
        card->add(dn_btn);
    }

    // ── Network list rows ───────────────────────────────────────────────────
    for (int i = 0; i < visible; i++) {
        const auto& net = (*nets)[scroll_offset + i];
        int row_y = row_start_y + i * row_h;

        string label = net.ssid;
        (*network_map)[label] = net;

        Color text_color = net.connected ? dt::kGreen : dt::kTextPrimary;

        auto row_frame = make_shared<Frame>(
            Rect(0, row_y, card_w, row_h));
        row_frame->color(Palette::ColorId::bg, dt::kTransparent);
        row_frame->border(0);
        card->add(row_frame);

        // SSID text (left side)
        auto ssid_btn = make_shared<Button>(label,
            Rect(text_pad, 4, card_w - 140, row_h - 8));
        ssid_btn->font(Font(dt::FONT_BODY, Font::Weight::bold));
        ssid_btn->color(Palette::ColorId::button_bg, dt::kTransparent);
        ssid_btn->color(Palette::ColorId::button_text, text_color);
        ssid_btn->border(0);

        ssid_btn->on_click([=](Event&) {
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
                        on_show_screen(create_wifi_settings_panel(
                            on_back, on_scan_wifi, on_connect,
                            on_item_selected, on_show_screen,
                            scroll_offset, nets));
                    }
                });
            if (on_show_screen) on_show_screen(pwd_screen);
        });
        row_frame->add(ssid_btn);

        // WiFi signal indicator – arc icon
        auto wifi_icon = make_shared<WifiIcon>(
            Rect(card_w - 110, (row_h - 30) / 2, 30, 30), text_color);
        row_frame->add(wifi_icon);

        // Chevron – circular button with ">"
        auto chev_bg = make_shared<Frame>(
            Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
        chev_bg->fill_flags({Theme::FillFlag::blend});
        chev_bg->color(Palette::ColorId::bg,
            net.connected ? dt::kGreen : dt::kGrayLight);
        chev_bg->border(0);
        chev_bg->border_radius(13);
        row_frame->add(chev_bg);

        auto chevron = make_shared<Label>(">",
            Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
        chevron->font(Font(14, Font::Weight::bold));
        chevron->color(Palette::ColorId::label_text, dt::kWhite);
        row_frame->add(chevron);

        // Separator line
        auto sep = make_shared<Frame>(
            Rect(10, row_y + row_h - 1, card_w - 20, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, dt::kGrayLight);
        sep->border(0);
        card->add(sep);
    }

    // ── "Other..." entry (always at the bottom of visible rows) ─────────────
    int other_y = row_start_y + visible * row_h;
    auto other_frame = make_shared<Frame>(
        Rect(0, other_y, card_w, row_h));
    other_frame->color(Palette::ColorId::bg, dt::kTransparent);
    other_frame->border(0);
    card->add(other_frame);

    auto other_btn = make_shared<Button>("Other...",
        Rect(text_pad, 4, card_w - 140, row_h - 8));
    other_btn->font(Font(dt::FONT_BODY, Font::Weight::bold));
    other_btn->color(Palette::ColorId::button_bg, dt::kTransparent);
    other_btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    other_btn->border(0);

    other_btn->on_click([=](Event&) {
        auto ssid_screen = create_password_prompt_screen(
            "Other Network",
            "Enter the network name (SSID)",
            "Next", "Back",
            [=](const string& ssid) {
                auto pwd_screen = create_password_prompt_screen(
                    "Network: " + ssid,
                    "Enter password to join",
                    "Join", "Back",
                    [=](const string& password) {
                        on_connect(ssid, password);
                    },
                    [=]() {
                        if (on_show_screen) {
                            on_show_screen(create_wifi_settings_panel(
                                on_back, on_scan_wifi, on_connect,
                                on_item_selected, on_show_screen));
                        }
                    });
                if (on_show_screen) on_show_screen(pwd_screen);
            },
            [=]() {
                if (on_show_screen) {
                    on_show_screen(create_wifi_settings_panel(
                        on_back, on_scan_wifi, on_connect,
                        on_item_selected, on_show_screen));
                }
            });
        if (on_show_screen) on_show_screen(ssid_screen);
    });

    other_frame->add(other_btn);

    auto other_chev_bg = make_shared<Frame>(
        Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
    other_chev_bg->fill_flags({Theme::FillFlag::blend});
    other_chev_bg->color(Palette::ColorId::bg, dt::kGrayLight);
    other_chev_bg->border(0);
    other_chev_bg->border_radius(13);
    other_frame->add(other_chev_bg);

    auto other_chevron = make_shared<Label>(">",
        Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
    other_chevron->font(Font(14, Font::Weight::bold));
    other_chevron->color(Palette::ColorId::label_text, dt::kWhite);
    other_frame->add(other_chevron);

    // ── Back button (bottom-left, "bt leave" style) ───────────────────────
    auto btn_back = make_shared<Button>("← Back",
        Rect(30, dt::SCREEN_H - 52, 111, 44));
    btn_back->font(Font(16, Font::Weight::bold));
    btn_back->color(Palette::ColorId::button_bg, dt::kWhite);
    btn_back->color(Palette::ColorId::button_text, dt::kAccentCyan);
    btn_back->color(Palette::ColorId::border, dt::kGrayLight);
    btn_back->border(2);
    btn_back->border_radius(4);
    btn_back->on_click([=](Event&) { if (on_back) on_back(); });
    container->add(btn_back);

    return container;
}