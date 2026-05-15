#include <egt/ui>
#include "screen_settings.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"
#include "../ui/brightness.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <memory>
#include <string>
#include <algorithm>

using namespace egt;
using namespace std;

// ── Settings screen — matches Figma node 2073:1996 (Jason-M4.0 v5) ─────────
// All px values below are the Figma frame (432×261) scaled ×1.852 → 800×484.
// Verified element-by-element against the JSON `absoluteBoundingBox` of each
// child (see commit message for the measurement methodology).

namespace {

// ── /etc-derived strings for the "About this device" line ─────────────────
string read_pipe(const string& cmd) {
    array<char, 256> buf{};
    string out;
    unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return out;
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) out += buf.data();
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return out;
}

string get_ip_address() {
    string ip = read_pipe("ip -4 -o addr show eth0 2>/dev/null | awk '{print $4}' | cut -d/ -f1");
    if (ip.empty()) ip = read_pipe("hostname -I 2>/dev/null | awk '{print $1}'");
    return ip.empty() ? string("not connected") : ip;
}

string get_serial() {
    string s = read_pipe("cat /sys/firmware/devicetree/base/serial-number 2>/dev/null | tr -d '\\0'");
    if (s.empty()) s = read_pipe("cat /etc/machine-id 2>/dev/null");
    if (s.empty()) return "LCA-00124";
    return s.size() > 12 ? s.substr(0, 12) : s;
}

string get_firmware_version() {
    string v = read_pipe("grep -h ^VERSION_ID= /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"'");
    return v.empty() ? string("1.0.0") : v;
}

// ── Stylised sun icon ──────────────────────────────────────────────────────
// Figma "Group 264/268" — both 28×28 at the brightness card. The small sun
// has shorter rays to read as a "lower brightness" affordance.
class SunIcon : public Widget {
public:
    SunIcon(const Rect& rect, float ray_factor = 1.0f)
        : Widget(rect), m_ray_factor(ray_factor)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        float dim = static_cast<float>(min(b.width(), b.height()));
        auto center = b.center();
        const float disc_r  = dim * 0.18f;
        const float ray_in  = dim * (0.27f * m_ray_factor + 0.06f);
        const float ray_out = dim * (0.45f * m_ray_factor + 0.05f);

        painter.set(dt::kTextPrimary);
        painter.draw(Arc(center, disc_r, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();

        painter.line_width(1.5f);
        for (int i = 0; i < 8; ++i) {
            float a = static_cast<float>(i) * (2.0f * static_cast<float>(M_PI) / 8.0f);
            float cx = static_cast<float>(center.x());
            float cy = static_cast<float>(center.y());
            painter.draw(Line(
                Point(cx + ray_in  * std::cos(a), cy + ray_in  * std::sin(a)),
                Point(cx + ray_out * std::cos(a), cy + ray_out * std::sin(a))));
            painter.stroke();
        }
    }

private:
    float m_ray_factor;
};

// ── Wi-Fi glyph (concentric arcs) sized to fit inside a 72×72 circle ──────
class WifiGlyph : public Widget {
public:
    explicit WifiGlyph(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        float dim = static_cast<float>(min(b.width(), b.height()));
        auto center_pt = Point(b.x() + b.width() / 2,
                               b.y() + b.height() * 0.74f);
        constexpr float start = -static_cast<float>(M_PI) * 0.75f;
        constexpr float end   = -static_cast<float>(M_PI) * 0.25f;
        float radii[] = {dim * 0.16f, dim * 0.30f, dim * 0.44f};

        painter.set(dt::kTextPrimary);
        painter.line_width(2.5f);
        for (float r : radii) {
            painter.draw(Arc(center_pt, r, start, end));
            painter.stroke();
        }
        painter.draw(Arc(center_pt, dim * 0.045f, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
    }
};

// ── Ethernet plug glyph (body + 3 pins + cable stub) ───────────────────────
class EthernetGlyph : public Widget {
public:
    explicit EthernetGlyph(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        float dim = static_cast<float>(min(b.width(), b.height()));
        float cx = b.x() + b.width()  / 2.0f;
        float cy = b.y() + b.height() / 2.0f + dim * 0.06f;

        const float body_w = dim * 0.50f;
        const float body_h = dim * 0.40f;
        const float bx = cx - body_w / 2.0f;
        const float by = cy - body_h / 2.0f;

        painter.set(dt::kTextPrimary);
        painter.line_width(2.2f);

        // Body rectangle (4 sides)
        painter.draw(Line(Point(bx,          by),          Point(bx + body_w, by)));
        painter.stroke();
        painter.draw(Line(Point(bx + body_w, by),          Point(bx + body_w, by + body_h)));
        painter.stroke();
        painter.draw(Line(Point(bx + body_w, by + body_h), Point(bx,          by + body_h)));
        painter.stroke();
        painter.draw(Line(Point(bx,          by + body_h), Point(bx,          by)));
        painter.stroke();

        // 3 contact pins above
        const float pin_h = body_h * 0.55f;
        for (int i = 0; i < 3; ++i) {
            float px = bx + body_w * (0.25f + 0.25f * static_cast<float>(i));
            painter.draw(Line(Point(px, by), Point(px, by - pin_h)));
            painter.stroke();
        }

        // Cable stub
        painter.draw(Line(Point(cx, by + body_h),
                          Point(cx, by + body_h + dim * 0.12f)));
        painter.stroke();
    }
};

// ── Chevron glyph (`<`) — used inside the Back-button circle ───────────────
class ChevronLeft : public Widget {
public:
    explicit ChevronLeft(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        float cx = b.x() + b.width()  / 2.0f;
        float cy = b.y() + b.height() / 2.0f;
        float sz = static_cast<float>(min(b.width(), b.height())) * 0.26f;

        painter.set(dt::kTextPrimary);
        painter.line_width(2.5f);
        painter.draw(Line(Point(cx + sz * 0.6f, cy - sz),
                          Point(cx - sz * 0.6f, cy)));
        painter.stroke();
        painter.draw(Line(Point(cx - sz * 0.6f, cy),
                          Point(cx + sz * 0.6f, cy + sz)));
        painter.stroke();
    }
};

// ── Card helper — gray rounded background, ~Figma "Rectangle 68/96/97" ─────
shared_ptr<Frame> make_section_card(int x, int y, int w, int h)
{
    auto card = make_shared<Frame>(Rect(x, y, w, h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kGrayBg);
    card->border(0);
    card->border_radius(dt::RADIUS_LG);
    return card;
}

// ── Brightness bar — composite slider matching Figma exactly ───────────────
// Figma: track 409×20 rounded full-pill, green fill from start of track, 28×28
// green handle on top of the fill's right edge. The default egt::Slider has
// a thin track and a disproportionately large handle, so we composite three
// Frames + a drag handler instead.
struct BrightnessBar {
    shared_ptr<Frame> root;        // the 409×28 hit area (track height + handle overhang)
    function<void(int)> set_value; // call to programmatically move the bar
};

BrightnessBar make_brightness_bar(int x, int y, int width, int initial_value)
{
    BrightnessBar bb;
    const int track_h = 20;
    const int handle_d = 28;
    const int total_h = handle_d;                  // handle slightly overlaps track ends
    const int handle_r = handle_d / 2;
    const int track_y_in = (total_h - track_h) / 2;
    // The handle's centre can travel from x=handle_r to x=width-handle_r so it
    // never clips off the bar bounds.
    const int travel = width - handle_d;

    auto root = make_shared<Frame>(Rect(x, y, width, total_h));
    root->fill_flags({Theme::FillFlag::blend});
    root->color(Palette::ColorId::bg, dt::kTransparent);
    root->border(0);
    bb.root = root;

    // Track background — pill, full width
    auto track_bg = make_shared<Frame>(Rect(0, track_y_in, width, track_h));
    track_bg->fill_flags({Theme::FillFlag::blend});
    track_bg->color(Palette::ColorId::bg, palette::kGray200);
    track_bg->border(0);
    track_bg->border_radius(track_h / 2);
    root->add(track_bg);

    // Green fill (width tracks value 0..100)
    auto fill = make_shared<Frame>(Rect(0, track_y_in, 0, track_h));
    fill->fill_flags({Theme::FillFlag::blend});
    fill->color(Palette::ColorId::bg, dt::kGreen);
    fill->border(0);
    fill->border_radius(track_h / 2);
    root->add(fill);

    // Handle circle (positioned to sit at the green fill's right edge)
    auto handle = make_shared<Frame>(Rect(0, 0, handle_d, handle_d));
    handle->fill_flags({Theme::FillFlag::blend});
    handle->color(Palette::ColorId::bg, dt::kGreen);
    handle->border(2);
    handle->color(Palette::ColorId::border, palette::kSliderHandleBorder);
    handle->border_radius(handle_r);
    root->add(handle);

    auto current = make_shared<int>(initial_value);

    auto layout = [=](int value) {
        value = std::max(0, std::min(100, value));
        *current = value;
        // Fill width: 0..(width - handle_d) so the rounded fill end always
        // sits under the handle. Plus handle_r so the fill caps under the
        // handle's vertical centre.
        int fill_w = (travel * value) / 100 + handle_r;
        fill->resize(Size(fill_w, track_h));
        int handle_x = (travel * value) / 100;
        handle->move(Point(handle_x, 0));
    };

    layout(initial_value);
    bb.set_value = layout;

    // Drag-anywhere-on-the-bar interaction
    auto drag_to_x = [=](int screen_x) {
        int local_x = screen_x - root->box().x() - handle_r;
        local_x = std::max(0, std::min(travel, local_x));
        int v = (local_x * 100) / std::max(1, travel);
        layout(v);
        ui::set_brightness(v);
    };

    root->on_event([=](Event& e) {
        drag_to_x(e.pointer().point.x());
        handle->color(Palette::ColorId::bg, palette::kSliderHandlePressed);
        handle->damage();
    }, {EventId::pointer_drag_start});

    root->on_event([=](Event& e) {
        drag_to_x(e.pointer().point.x());
    }, {EventId::pointer_drag});

    root->on_event([=](Event&) {
        handle->color(Palette::ColorId::bg, dt::kGreen);
        handle->damage();
    }, {EventId::pointer_drag_stop});

    // Single tap anywhere on the bar jumps the handle there
    root->on_event([=](Event& e) {
        drag_to_x(e.pointer().point.x());
    }, {EventId::pointer_click});

    return bb;
}

} // namespace

// ── Public entry point ─────────────────────────────────────────────────────
shared_ptr<Widget> create_settings_screen(
    function<void()> on_back,
    function<void()> on_wifi_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Common section X (matches Figma left margin ≈ 81 px)
    const int section_x = 80;

    // ── 1) Screen Brightness ────────────────────────────────────────────────
    // Title "Screen Brightness" — Figma (81, 31) 228×33  Gothic A1 Bold 14pt
    auto bright_title = make_shared<Label>("Screen Brightness",
        Rect(section_x, 31, 300, 33));
    bright_title->font(Font(15, Font::Weight::bold));
    bright_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    bright_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(bright_title);

    // Card "Rectangle 68" — Figma (78, 70) 628×87
    const int brt_card_x = 78;
    const int brt_card_y = 70;
    const int brt_card_w = 628;
    const int brt_card_h = 87;
    auto brt_card = make_section_card(brt_card_x, brt_card_y, brt_card_w, brt_card_h);
    container->add(brt_card);

    // Sun small at card-relative (53, 28) 28×28
    auto sun_small = make_shared<SunIcon>(Rect(53, 28, 28, 28), 0.55f);
    brt_card->add(sun_small);
    // Sun big at card-relative (535, 28) 28×28
    auto sun_big = make_shared<SunIcon>(Rect(535, 28, 28, 28), 1.0f);
    brt_card->add(sun_big);

    // Brightness bar — Figma slider track at card-relative (105, 32) 409×20.
    // The bar widget reserves room for a 28 px handle, so the bar height is
    // 28 and we place it vertically centred on the track's y axis.
    int cur_brightness = ui::get_brightness();
    if (cur_brightness < 0) cur_brightness = 80;
    auto bar = make_brightness_bar(/*x=*/105, /*y=*/(brt_card_h - 28) / 2, /*w=*/409, cur_brightness);
    brt_card->add(bar.root);

    // ── 2) Internet Connection ──────────────────────────────────────────────
    // Title — Figma (81, 163) 246×33
    auto inet_title = make_shared<Label>("Internet Connection",
        Rect(section_x, 163, 320, 33));
    inet_title->font(Font(15, Font::Weight::bold));
    inet_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    inet_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(inet_title);

    // Cards: Wi-Fi at (81, 206) 306×113, Ethernet at (400, 206) 306×113
    const int chip_y = 206;
    const int chip_w = 306;
    const int chip_h = 113;
    const int wifi_x = 81;
    const int eth_x  = 400;

    auto add_internet_card = [&](int card_x, const string& title_text, bool is_wifi) {
        auto card = make_section_card(card_x, chip_y, chip_w, chip_h);
        container->add(card);

        // Gray ellipse background — Figma "Ellipse 8" 72×72 at card-relative (56, 20)
        const int circle_d = 72;
        auto circle_bg = make_shared<Frame>(Rect(56, 20, circle_d, circle_d));
        circle_bg->fill_flags({Theme::FillFlag::blend});
        circle_bg->color(Palette::ColorId::bg, palette::kGray200);
        circle_bg->border(0);
        circle_bg->border_radius(circle_d / 2);
        card->add(circle_bg);

        // Glyph centred inside the circle
        if (is_wifi) {
            auto g = make_shared<WifiGlyph>(Rect(56, 20, circle_d, circle_d));
            card->add(g);
        } else {
            auto g = make_shared<EthernetGlyph>(Rect(56, 20, circle_d, circle_d));
            card->add(g);
        }

        // Text — Figma "Wi-Fi" at card-relative (143, 44); "Ethernet" at (144, 44)
        auto lbl = make_shared<Label>(title_text,
            Rect(143, 0, chip_w - 143 - 16, chip_h));
        lbl->font(Font(15, Font::Weight::bold));
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
        card->add(lbl);

        return card;
    };

    auto wifi_card = add_internet_card(wifi_x, "Wi-Fi",   true);
    auto eth_card  = add_internet_card(eth_x,  "Ethernet", false);

    wifi_card->on_event([on_wifi_settings](Event&) {
        if (on_wifi_settings) on_wifi_settings();
    }, {EventId::pointer_click});

    // ── 3) About this device ────────────────────────────────────────────────
    // Title — Figma (81, 322) 217×33
    auto about_title = make_shared<Label>("About this device",
        Rect(section_x, 322, 320, 33));
    about_title->font(Font(15, Font::Weight::bold));
    about_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    about_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(about_title);

    // Body — Figma (81, 361) 522×28  Gothic A1 Regular 12pt
    const string about_text =
        "Firmware v" + get_firmware_version() +
        "  ·  Serial #" + get_serial() +
        "  ·  IP " + get_ip_address();
    auto about_body = make_shared<Label>(about_text,
        Rect(section_x, 361, 640, 28));
    about_body->font(Font(12, Font::Weight::normal));
    about_body->color(Palette::ColorId::label_text, palette::kGray600);
    about_body->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(about_body);

    // ── Back button ─────────────────────────────────────────────────────────
    // Group at Figma (27, 414) 152×46. Inside: 46×46 circle on the left with
    // a chevron, and the label "Back" (Gothic A1 Bold 14pt) to its right.
    const int back_y = 414;
    const int circle_d = 46;

    auto back_circle = make_shared<Frame>(Rect(27, back_y, circle_d, circle_d));
    back_circle->fill_flags({Theme::FillFlag::blend});
    back_circle->color(Palette::ColorId::bg, palette::kGray200);
    back_circle->border(0);
    back_circle->border_radius(circle_d / 2);
    container->add(back_circle);

    auto back_chev = make_shared<ChevronLeft>(Rect(27, back_y, circle_d, circle_d));
    container->add(back_chev);

    auto back_lbl = make_shared<Label>("Back",
        Rect(49 + circle_d, back_y, 120, circle_d));
    back_lbl->font(Font(15, Font::Weight::bold));
    back_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    back_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(back_lbl);

    // Single hit zone covering circle + label
    auto back_hit = make_shared<Frame>(Rect(20, back_y - 4, 200, circle_d + 8));
    back_hit->fill_flags({Theme::FillFlag::blend});
    back_hit->color(Palette::ColorId::bg, dt::kTransparent);
    back_hit->border(0);
    container->add(back_hit);
    back_hit->on_event([on_back](Event&) {
        if (on_back) on_back();
    }, {EventId::pointer_click});

    return container;
}
