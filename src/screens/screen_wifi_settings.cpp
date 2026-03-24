#include <egt/ui>
#include <egt/widget.h>
#include <egt/view.h>
#include <egt/keycode.h>

#include "screen_wifi_settings.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <map>
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace egt;
using namespace egt_wifi;
using namespace std;

// ── WiFi signal-arc icon (3 concentric arcs + dot) ──────────────────────────
class WifiIcon : public Widget {
public:
    /// @param signal  0-100 RSSI percentage
    WifiIcon(const Rect& rect, const Color& col, int signal)
        : Widget(rect), m_color(col), m_signal(signal)
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

        // How many arcs to light up based on signal strength
        // 0-25  → 0 arcs (dot only)
        // 26-50 → 1 arc
        // 51-75 → 2 arcs
        // 76+   → 3 arcs
        int active = (m_signal > 75) ? 3 : (m_signal > 50) ? 2 : (m_signal > 25) ? 1 : 0;

        Color dim(m_color.red(), m_color.green(), m_color.blue(), 60);

        float radii[] = {7.0f, 13.0f, 19.0f};
        painter.line_width(2.5f);
        for (int i = 0; i < 3; i++) {
            painter.set(i < active ? m_color : dim);
            painter.draw(Arc(center, radii[i], start, end));
            painter.stroke();
        }

        // small dot at the base (always solid)
        painter.set(m_color);
        painter.draw(Arc(center, 2.0f, 0.0f, 2.0f * M_PI));
        painter.fill();
    }

private:
    Color m_color;
    int m_signal;

public:
    void set_color(const Color& c) { m_color = c; damage(); }
};

std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect,
    function<void(const egt_wifi::WiFiNetwork&)> on_item_selected,
    function<void(std::shared_ptr<egt::Widget>)> on_show_screen,
    std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> cached_networks)
{
    // ── Scan or use cached networks ─────────────────────────────────────────
    auto nets = cached_networks;
    if (!nets) {
        egt_wifi::WiFiManager wifi;
        nets = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());
    }
    // Sort by signal strength (strongest first)
    sort(nets->begin(), nets->end(),
         [](const WiFiNetwork& a, const WiFiNetwork& b) {
             return a.signal > b.signal;
         });
    const int total = static_cast<int>(nets->size());

    auto network_map = make_shared<map<string, egt_wifi::WiFiNetwork>>();
    auto alive = make_shared<bool>(true);

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kGrayBg);

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
    const int card_h = dt::SCREEN_H - 100; // 380

    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->border(0);
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

    // ── Scrollable network list ─────────────────────────────────────────────
    const int list_top = 52;
    const int list_h = card_h - list_top;
    const int row_h = 65;
    const int text_pad = 20;

    // Total rows: networks + "Other..." entry
    const int total_rows = total + 1;
    const int content_h = total_rows * row_h;

    auto scroll_view = make_shared<ScrolledView>(
        Rect(0, list_top, card_w, list_h),
        ScrolledView::Policy::never,     // no horizontal scroll
        ScrolledView::Policy::as_needed  // vertical scroll when content overflows
    );
    scroll_view->fill_flags({Theme::FillFlag::blend});
    scroll_view->color(Palette::ColorId::bg, dt::kTransparent);
    scroll_view->color(Palette::ColorId::button_bg, dt::kTransparent); // hide scrollbar
    scroll_view->color(Palette::ColorId::button_fg, dt::kTransparent);
    scroll_view->color(Palette::ColorId::border, dt::kTransparent);
    scroll_view->slider_dim(0);
    scroll_view->border(0);
    card->add(scroll_view);

    // Arrow-key scrolling (simulator convenience — real device uses touch drag)
    container->on_event([scroll_view, row_h](Event& event) {
        auto key = event.key().keycode;
        if (key == EKEY_DOWN)
            scroll_view->voffset(scroll_view->voffset() - row_h);
        else if (key == EKEY_UP)
            scroll_view->voffset(scroll_view->voffset() + row_h);
        else
            return;
        event.stop();
    }, {EventId::keyboard_down});

    // Content frame inside the scrolled view — holds all rows
    auto list_content = make_shared<Frame>(
        Rect(0, 0, card_w, content_h));
    list_content->fill_flags({Theme::FillFlag::blend});
    list_content->color(Palette::ColorId::bg, dt::kTransparent);
    list_content->border(0);
    scroll_view->add(list_content);

    // ── Network list rows ───────────────────────────────────────────────────
    for (int i = 0; i < total; i++) {
        const auto& net = (*nets)[i];
        int row_y = i * row_h;

        string label = net.ssid;
        (*network_map)[label] = net;

        Color text_color = net.connected ? dt::kGreen : dt::kTextPrimary;

        auto row_frame = make_shared<Frame>(
            Rect(0, row_y, card_w, row_h));
        row_frame->fill_flags({Theme::FillFlag::blend});
        row_frame->color(Palette::ColorId::bg, dt::kTransparent);
        row_frame->border(0);
        list_content->add(row_frame);

        // SSID text (left-aligned) — use Label, not Button, so drag events
        // pass through to ScrolledView for scroll on the whole row
        auto ssid_lbl = make_shared<Label>(label,
            Rect(text_pad, 4, card_w - 140, row_h - 8));
        ssid_lbl->font(Font(dt::FONT_BODY, Font::Weight::bold));
        ssid_lbl->color(Palette::ColorId::label_text, text_color);
        ssid_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
        ssid_lbl->border(0);
        row_frame->add(ssid_lbl);

        // WiFi signal indicator – arc icon (arcs reflect signal strength)
        auto wifi_icon = make_shared<WifiIcon>(
            Rect(card_w - 110, (row_h - 30) / 2, 30, 30), text_color, net.signal);
        row_frame->add(wifi_icon);

        // Chevron – white circle with shadow + colored ">" stroke
        auto chev_shadow = make_shared<Frame>(
            Rect(card_w - 54, (row_h - 26) / 2 + 1, 26, 26));
        chev_shadow->fill_flags({Theme::FillFlag::blend});
        chev_shadow->color(Palette::ColorId::bg, Color(0, 0, 0, 40));
        chev_shadow->border(0);
        chev_shadow->border_radius(13);
        row_frame->add(chev_shadow);

        auto chev_bg = make_shared<Frame>(
            Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
        chev_bg->fill_flags({Theme::FillFlag::blend});
        chev_bg->color(Palette::ColorId::bg, dt::kWhite);
        chev_bg->border(0);
        chev_bg->border_radius(13);
        row_frame->add(chev_bg);

        auto chevron = make_shared<Label>(">",
            Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
        chevron->font(Font(14, Font::Weight::bold));
        chevron->color(Palette::ColorId::label_text, text_color);
        row_frame->add(chevron);

        // --- Green highlight on touch (text + icon + chevron) ---
        auto hover_timer = make_shared<Timer>(chrono::milliseconds(1500));
        hover_timer->on_timeout([=]() {
            ssid_lbl->color(Palette::ColorId::label_text, text_color);
            wifi_icon->set_color(text_color);
            chevron->color(Palette::ColorId::label_text, text_color);
            row_frame->damage();
        });
        row_frame->on_event([=](Event& event) {
            if (event.id() == EventId::raw_pointer_down) {
                hover_timer->stop();
                ssid_lbl->color(Palette::ColorId::label_text, dt::kGreen);
                wifi_icon->set_color(dt::kGreen);
                chevron->color(Palette::ColorId::label_text, dt::kGreen);
                row_frame->damage();
            } else if (event.id() == EventId::raw_pointer_up) {
                hover_timer->start();
            } else if (event.id() == EventId::pointer_drag_start ||
                       event.id() == EventId::pointer_drag) {
                hover_timer->stop();
                ssid_lbl->color(Palette::ColorId::label_text, text_color);
                wifi_icon->set_color(text_color);
                chevron->color(Palette::ColorId::label_text, text_color);
                row_frame->damage();
            }
        });

        // Tap anywhere on the row → open password prompt
        row_frame->on_event([=](Event&) {
            *alive = false;
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
                            on_item_selected, on_show_screen, nets));
                    }
                });
            if (on_show_screen) on_show_screen(pwd_screen);
        }, {EventId::pointer_click});

        // Separator line (white — subtle, matches Figma)
        auto sep = make_shared<Frame>(
            Rect(10, row_h - 1, card_w - 20, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, dt::kWhite);
        sep->border(0);
        row_frame->add(sep);
    }

    // ── "Other..." entry (at the bottom of the list) ────────────────────────
    int other_y = total * row_h;
    auto other_frame = make_shared<Frame>(
        Rect(0, other_y, card_w, row_h));
    other_frame->fill_flags({Theme::FillFlag::blend});
    other_frame->color(Palette::ColorId::bg, dt::kTransparent);
    other_frame->border(0);

    list_content->add(other_frame);

    auto other_lbl = make_shared<Label>("Other...",
        Rect(text_pad, 4, card_w - 140, row_h - 8));
    other_lbl->font(Font(dt::FONT_BODY, Font::Weight::bold));
    other_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    other_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    other_lbl->border(0);
    other_frame->add(other_lbl);

    other_frame->on_event([=](Event&) {
        *alive = false;
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
    }, {EventId::pointer_click});

    auto other_chev_shadow = make_shared<Frame>(
        Rect(card_w - 54, (row_h - 26) / 2 + 1, 26, 26));
    other_chev_shadow->fill_flags({Theme::FillFlag::blend});
    other_chev_shadow->color(Palette::ColorId::bg, Color(0, 0, 0, 40));
    other_chev_shadow->border(0);
    other_chev_shadow->border_radius(13);
    other_frame->add(other_chev_shadow);

    auto other_chev_bg = make_shared<Frame>(
        Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
    other_chev_bg->fill_flags({Theme::FillFlag::blend});
    other_chev_bg->color(Palette::ColorId::bg, dt::kWhite);
    other_chev_bg->border(0);
    other_chev_bg->border_radius(13);
    other_frame->add(other_chev_bg);

    auto other_chevron = make_shared<Label>(">",
        Rect(card_w - 55, (row_h - 26) / 2, 26, 26));
    other_chevron->font(Font(14, Font::Weight::bold));
    other_chevron->color(Palette::ColorId::label_text, dt::kTextPrimary);
    other_frame->add(other_chevron);

    // ── Auto-retry scan when no networks found ──────────────────────────────
    if (nets->empty() && on_show_screen) {
        auto scan_label = make_shared<Label>("Scanning for networks...",
            Rect(30, other_y + row_h + 5, 400, 28));
        scan_label->font(Font(16, Font::Weight::normal));
        scan_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
        list_content->add(scan_label);

        auto retry_count = make_shared<int>(0);
        auto retry_timer = make_shared<PeriodicTimer>(chrono::milliseconds(3000));
        retry_timer->on_timeout([=]() {
            if (!*alive) { retry_timer->cancel(); return; }
            (*retry_count)++;
            printf("[WIFI_SETTINGS] auto-retry scan %d/10\n", *retry_count);
            fflush(stdout);
            if (*retry_count > 10) {
                retry_timer->cancel();
                scan_label->text("No networks found");
                return;
            }
            WiFiManager wifi;
            auto fresh = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());
            if (!fresh->empty()) {
                retry_timer->cancel();
                *alive = false;
                on_show_screen(create_wifi_settings_panel(
                    on_back, on_scan_wifi, on_connect,
                    on_item_selected, on_show_screen, fresh));
            }
        });
        retry_timer->start();
    }

    // ── Periodic refresh of the network list (every 15 s) ───────────────────
    if (!nets->empty() && on_show_screen) {
        auto refresh_timer = make_shared<PeriodicTimer>(chrono::seconds(15));
        refresh_timer->on_timeout([=]() {
            if (!*alive) { refresh_timer->cancel(); return; }
            WiFiManager wifi;
            auto fresh = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());

            // Build simple fingerprint: sorted "ssid:connected," for each network
            auto fingerprint = [](const vector<WiFiNetwork>& v) {
                vector<string> parts;
                parts.reserve(v.size());
                for (const auto& n : v)
                    parts.push_back(n.ssid + ":" + (n.connected ? "1" : "0"));
                sort(parts.begin(), parts.end());
                string fp;
                for (const auto& p : parts) fp += p + ",";
                return fp;
            };

            if (fingerprint(*fresh) != fingerprint(*nets)) {
                printf("[WIFI_SETTINGS] network list changed — refreshing\n");
                fflush(stdout);
                refresh_timer->cancel();
                *alive = false;
                on_show_screen(create_wifi_settings_panel(
                    on_back, on_scan_wifi, on_connect,
                    on_item_selected, on_show_screen, fresh));
            }
        });
        refresh_timer->start();
    }

    return container;
}