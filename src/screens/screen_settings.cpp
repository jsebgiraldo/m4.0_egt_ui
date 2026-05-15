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

// ── Sun icon ───────────────────────────────────────────────────────────────
// Figma "Group 264" / "Group 268" — both 15×15 pt (28×28 px scaled). The
// geometry is identical for both; the design encodes "low / high brightness"
// via colour and stroke weight, not size. Default ctor renders dark/bold for
// "max"; pass `light` to get the thin / pale "min" variant on the other end
// of the slider.
class SunIcon : public Widget {
public:
    SunIcon(const Rect& rect, bool light = false)
        : Widget(rect), m_light(light)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        float dim = static_cast<float>(min(b.width(), b.height()));
        auto center = b.center();
        const float disc_r  = dim * 0.21f;
        const float ray_in  = dim * 0.32f;
        const float ray_out = dim * 0.48f;

        const Color& col   = m_light ? palette::kGray500 : dt::kTextPrimary;
        const float stroke = m_light ? 1.2f : 2.4f;

        painter.set(col);
        painter.draw(Arc(center, disc_r, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();

        painter.line_width(stroke);
        const float cx = static_cast<float>(center.x());
        const float cy = static_cast<float>(center.y());
        for (int i = 0; i < 8; ++i) {
            float a = static_cast<float>(i) * (2.0f * static_cast<float>(M_PI) / 8.0f);
            painter.draw(Line(
                Point(cx + ray_in  * std::cos(a), cy + ray_in  * std::sin(a)),
                Point(cx + ray_out * std::cos(a), cy + ray_out * std::sin(a))));
            painter.stroke();
        }
    }

private:
    bool m_light;
};

// ── Wi-Fi glyph ────────────────────────────────────────────────────────────
// Figma "Group 127" — 24.4×18 pt (45×33 px). Sits inside the 72×72 gray
// circle background, centred horizontally and a bit above the visual centre
// so the base dot lands ~60 % down. Three concentric arcs opening upward +
// a small filled dot at the bottom.
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
        float w = static_cast<float>(b.width());
        float h = static_cast<float>(b.height());
        // Anchor: the bottom of the wifi glyph (the dot) sits at ~85 % of h
        // so the upward-opening arcs fit nicely.
        auto pivot = Point(b.x() + b.width() / 2,
                           b.y() + static_cast<int>(b.height() * 0.85f));
        constexpr float start = -static_cast<float>(M_PI) * 0.75f;   // −135°
        constexpr float end   = -static_cast<float>(M_PI) * 0.25f;   //  −45°

        // Three concentric arcs: the outermost roughly matches the glyph
        // width; inner arcs scale down proportionally.
        const float outer = w * 0.48f;
        const float mid   = w * 0.32f;
        const float inner = w * 0.16f;

        painter.set(dt::kTextPrimary);
        painter.line_width(std::max(3.0f, h * 0.10f));
        for (float r : {outer, mid, inner}) {
            painter.draw(Arc(pivot, r, start, end));
            painter.stroke();
        }
        // Base dot
        painter.draw(Arc(pivot, std::max(2.0f, h * 0.07f),
                         0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
    }
};

// ── Ethernet plug (RJ45) glyph ─────────────────────────────────────────────
// Figma "Group 269" — 25.5×24.7 pt (47×46 px) inside the 72×72 circle.
// Drawn as a flat RJ45 jack silhouette: an outlined rectangle for the
// connector body, a small notched tab in the middle, three pin strokes
// inside, and a cable stub coming out the bottom. Stroke widths are scaled
// from the icon size so the lines read cleanly at 800×480 and at the SAMA
// panel native scale.
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
        float w = static_cast<float>(b.width());
        float h = static_cast<float>(b.height());
        float cx = b.x() + w / 2.0f;
        float cy = b.y() + h * 0.48f;

        // Connector body proportions — wider than tall, like an RJ45 jack.
        const float body_w = w * 0.55f;
        const float body_h = h * 0.36f;
        const float bx = cx - body_w / 2.0f;
        const float by = cy - body_h / 2.0f;
        const float stroke = std::max(2.5f, std::min(w, h) * 0.07f);

        painter.set(dt::kTextPrimary);
        painter.line_width(stroke);

        // Body rectangle (top side has a notch — see below)
        painter.draw(Line(Point(bx + body_w, by),          Point(bx + body_w, by + body_h)));
        painter.stroke();
        painter.draw(Line(Point(bx + body_w, by + body_h), Point(bx,          by + body_h)));
        painter.stroke();
        painter.draw(Line(Point(bx,          by + body_h), Point(bx,          by)));
        painter.stroke();
        // Top with central notch (locking tab)
        const float notch_w = body_w * 0.30f;
        const float notch_h = body_h * 0.28f;
        painter.draw(Line(Point(bx,                          by),          Point(cx - notch_w / 2, by)));
        painter.stroke();
        painter.draw(Line(Point(cx - notch_w / 2,            by),          Point(cx - notch_w / 2, by - notch_h)));
        painter.stroke();
        painter.draw(Line(Point(cx - notch_w / 2,            by - notch_h),Point(cx + notch_w / 2, by - notch_h)));
        painter.stroke();
        painter.draw(Line(Point(cx + notch_w / 2,            by - notch_h),Point(cx + notch_w / 2, by)));
        painter.stroke();
        painter.draw(Line(Point(cx + notch_w / 2,            by),          Point(bx + body_w,      by)));
        painter.stroke();

        // 3 pin strokes inside the connector
        const float pin_top    = by + body_h * 0.30f;
        const float pin_bottom = by + body_h * 0.85f;
        for (int i = 0; i < 3; ++i) {
            float px = bx + body_w * (0.30f + 0.20f * static_cast<float>(i));
            painter.draw(Line(Point(px, pin_top), Point(px, pin_bottom)));
            painter.stroke();
        }

        // Cable stub coming out the bottom
        painter.draw(Line(Point(cx, by + body_h),
                          Point(cx, by + body_h + h * 0.16f)));
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
// Figma: 409×20 fat-pill track, green fill from the start, 28 px green handle
// at the fill's right edge. The default egt::Slider draws a thin track and a
// big handle — wrong visual language — so we composite the visual ourselves
// and handle drag events directly. The trick to make drag work from any
// nested parent is converting the event's display point back to widget-local
// coordinates with `to_display(Point(0,0))`.
struct BrightnessBar {
    shared_ptr<Frame> root;
};

BrightnessBar make_brightness_bar(int x, int y, int width, int initial_value)
{
    BrightnessBar bb;
    const int track_h  = 20;
    const int handle_d = 28;
    const int total_h  = handle_d;
    const int handle_r = handle_d / 2;
    const int track_y  = (total_h - track_h) / 2;
    const int travel   = width - handle_d;

    auto root = make_shared<Frame>(Rect(x, y, width, total_h));
    root->fill_flags({Theme::FillFlag::blend});
    root->color(Palette::ColorId::bg, dt::kTransparent);
    root->border(0);
    bb.root = root;

    // Gray pill — the inactive track
    auto track_bg = make_shared<Frame>(Rect(0, track_y, width, track_h));
    track_bg->fill_flags({Theme::FillFlag::blend});
    track_bg->color(Palette::ColorId::bg, palette::kGray200);
    track_bg->border(0);
    track_bg->border_radius(track_h / 2);
    root->add(track_bg);

    // Green fill — width tracks value 0..100
    auto fill = make_shared<Frame>(Rect(0, track_y, 0, track_h));
    fill->fill_flags({Theme::FillFlag::blend});
    fill->color(Palette::ColorId::bg, dt::kGreen);
    fill->border(0);
    fill->border_radius(track_h / 2);
    root->add(fill);

    // Handle circle — sits at the green fill's right edge
    auto handle = make_shared<Frame>(Rect(0, 0, handle_d, handle_d));
    handle->fill_flags({Theme::FillFlag::blend});
    handle->color(Palette::ColorId::bg, dt::kGreen);
    handle->border(2);
    handle->color(Palette::ColorId::border, palette::kSliderHandleBorder);
    handle->border_radius(handle_r);
    root->add(handle);

    auto layout = [=](int value) {
        value = std::max(0, std::min(100, value));
        int fill_w   = (travel * value) / 100 + handle_r;
        int handle_x = (travel * value) / 100;
        fill->resize(Size(fill_w, track_h));
        handle->move(Point(handle_x, 0));
    };
    layout(initial_value);

    // Invisible egt::Slider — owns the drag math + value semantics so we
    // don't try to reinvent pointer event tracking on a plain Frame. Same
    // pattern as the Age picker in screen_patient_info.cpp. To fully hide
    // its visual we have to override colours across ALL relevant GroupIds
    // (normal/active/disabled/checked) — otherwise the press-state colour
    // bleeds through during drag (the "red handle" we saw in v6).
    auto driver = make_shared<Slider>(
        Rect(0, 0, width, total_h),
        0, 100, initial_value, Orientation::horizontal);
    driver->slider_flags().set(Slider::SliderFlag::round_handle);
    driver->live_update(true);
    driver->border(0);

    for (auto group : {Palette::GroupId::normal, Palette::GroupId::active,
                       Palette::GroupId::disabled, Palette::GroupId::checked}) {
        driver->color(Palette::ColorId::bg,           dt::kTransparent, group);
        driver->color(Palette::ColorId::button_bg,    dt::kTransparent, group);
        driver->color(Palette::ColorId::button_fg,    dt::kTransparent, group);
        driver->color(Palette::ColorId::button_text,  dt::kTransparent, group);
        driver->color(Palette::ColorId::border,       dt::kTransparent, group);
        driver->color(Palette::ColorId::label_text,   dt::kTransparent, group);
        driver->color(Palette::ColorId::label_bg,     dt::kTransparent, group);
        driver->color(Palette::ColorId::text,         dt::kTransparent, group);
    }
    root->add(driver);

    driver->on_value_changed([=]() {
        int v = driver->value();
        layout(v);
        ui::set_brightness(v);
    });

    driver->on_event([=](Event&) {
        handle->color(Palette::ColorId::bg, palette::kSliderHandlePressed);
        handle->damage();
    }, {EventId::pointer_drag_start});

    driver->on_event([=](Event&) {
        handle->color(Palette::ColorId::bg, dt::kGreen);
        handle->damage();
    }, {EventId::pointer_drag_stop});

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

    // Sun icons — same geometry, different ink: pale-thin on the "low" side
    // and dark-bold on the "high" side to read as a brightness ramp.
    auto sun_left  = make_shared<SunIcon>(Rect( 53, 28, 28, 28), /*light=*/true);
    auto sun_right = make_shared<SunIcon>(Rect(535, 28, 28, 28), /*light=*/false);
    brt_card->add(sun_left);
    brt_card->add(sun_right);

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
        const int circle_x = 56;
        const int circle_y = 20;
        auto circle_bg = make_shared<Frame>(Rect(circle_x, circle_y, circle_d, circle_d));
        circle_bg->fill_flags({Theme::FillFlag::blend});
        circle_bg->color(Palette::ColorId::bg, palette::kGray200);
        circle_bg->border(0);
        circle_bg->border_radius(circle_d / 2);
        card->add(circle_bg);

        // Glyph sized per Figma, centred inside the gray circle.
        // Wi-Fi:  45×33 (Figma "Group 127") → offset (13, 18) inside the circle.
        // Eth:    47×46 (Figma "Group 269") → offset (13, 9) inside the circle.
        if (is_wifi) {
            auto g = make_shared<WifiGlyph>(
                Rect(circle_x + 13, circle_y + 18, 45, 33));
            card->add(g);
        } else {
            auto g = make_shared<EthernetGlyph>(
                Rect(circle_x + 13, circle_y + 9, 47, 46));
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

    // ── Back button — shared layout via ui::add_back_button ────────────────
    // Same chevron-in-circle + label at the canonical bottom-left position
    // (Figma 2073:1996, 27,414). Any screen that needs Back uses the same
    // helper so the button never drifts between screens.
    ui::add_back_button(*container, on_back);

    return container;
}
