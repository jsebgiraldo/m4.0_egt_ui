#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_settings.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"
#include "../ui/brightness.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
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

string nz(const string& s, const char* fallback = "—") {
    return s.empty() ? string(fallback) : s;
}

string get_model() {
    string m = read_pipe("cat /sys/firmware/devicetree/base/model 2>/dev/null | tr -d '\\0'");
    return nz(m, "SAMA5D27-WLSOM1-EK");
}

string get_kernel()   { return nz(read_pipe("uname -r 2>/dev/null")); }
string get_os_name()  { return nz(read_pipe("grep -h ^PRETTY_NAME= /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"'")); }
string get_hostname() { return nz(read_pipe("hostname 2>/dev/null")); }
string get_uptime()   { return nz(read_pipe("uptime -p 2>/dev/null | sed 's/^up //'")); }

string get_mac() {
    string m = read_pipe("cat /sys/class/net/eth0/address 2>/dev/null");
    if (m.empty()) m = read_pipe("cat /sys/class/net/wlan0/address 2>/dev/null");
    return nz(m);
}

string get_wifi_ssid() {
    string s = read_pipe("iwgetid -r 2>/dev/null");
    return s.empty() ? string("not connected") : s;
}

string get_memory() {
    // e.g. "212 / 495 MB used"
    return nz(read_pipe(
        "free -m 2>/dev/null | awk '/^Mem:/{printf \"%d / %d MB used\", $3, $2}'"));
}

string get_storage() {
    // e.g. "1.2G free of 3.5G"
    return nz(read_pipe(
        "df -h / 2>/dev/null | awk 'NR==2{print $4\" free of \"$2}'"));
}

string get_datetime() { return nz(read_pipe("date '+%Y-%m-%d %H:%M' 2>/dev/null")); }

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
// Section card with a subtle vertical gradient (Figma uses #F4F4F4 =
// rgb(244,244,244) at the top fading to #FFFFFF = rgb(255,255,255)).
class SectionGradient : public Widget {
public:
    SectionGradient(const Rect& r, float radius) : Widget(r), m_radius(radius)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& p, const Rect&) override
    {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = m_radius;
        const Color top{244, 244, 244};   // Figma #F4F4F4 (screen-local)
        const Color bot{255, 255, 255};   // Figma #FFFFFF
        Pattern grad(Pattern::StepArray{{0.0f, top}, {1.0f, bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        const auto PI = static_cast<float>(M_PI);
        p.draw(PointF(x + r, y));
        p.line(PointF(x + w - r, y));
        p.draw(Arc(PointF(x + w - r, y + r),     r, -PI / 2, 0.0f));
        p.line(PointF(x + w, y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f,    PI / 2));
        p.line(PointF(x + r, y + h));
        p.draw(Arc(PointF(x + r, y + h - r),     r, PI / 2,  PI));
        p.line(PointF(x, y + r));
        p.draw(Arc(PointF(x + r, y + r),         r, PI,      3 * PI / 2));
        p.set(grad);
        p.fill();
    }
private:
    float m_radius;
};

shared_ptr<Frame> make_section_card(int x, int y, int w, int h)
{
    // Wrap a transparent Frame that holds the gradient backdrop. Returning
    // a Frame keeps the existing API (callers add children to it).
    auto card = make_shared<Frame>(Rect(x, y, w, h));
    card->fill_flags({});  // transparent - the gradient does the painting
    card->border(0);
    // Figma corner radius is 8 figma px (brightness) / 7.6 (Wi-Fi, Ethernet)
    // -> ~14.8 / 14.1 device px. dt::RADIUS_LG (16) is 1-2 px rounder, so use
    // a screen-local 15 here instead of editing the shared token.
    constexpr float kCardRadius = 15.0f;
    card->add(make_shared<SectionGradient>(Rect(0, 0, w, h), kCardRadius));
    return card;
}

// ── ScrolledView with an invisible scrollbar ───────────────────────────────
// We want the page to scroll but without the loud default red slider showing.
// Policy::never disables scrolling entirely, so we keep `as_needed` (scroll
// works) and paint the vertical slider transparent. m_vslider is protected.
class CleanScrolledView : public ScrolledView {
public:
    explicit CleanScrolledView(const Rect& rect)
        : ScrolledView(rect, Policy::never, Policy::as_needed)
    {
        for (auto id : { Palette::ColorId::button_bg,
                         Palette::ColorId::button_fg,
                         Palette::ColorId::border,
                         Palette::ColorId::label_text }) {
            m_vslider.color(id, dt::kTransparent);
        }
    }
};

// ── Person / account glyph (SVG) ───────────────────────────────────────────
// Rendered as an SVG (like the Setup gear) rather than a font glyph so it is
// crisp at any scale and never depends on the device font. Ink #646469 matches
// dt::kTextPrimary, so it reads the same as the Wi-Fi / Ethernet glyphs.
static const char* kPersonSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z" fill="#646469"/>
</svg>)svg";

static Image load_person(int size) {
    // libegt 1.10 (target) heap-corruption fix: keep the SvgImage alive in
    // a static cache so the returned sliced-Image's backing buffer stays
    // valid (see screen_patient_info.cpp load_svg_icon for the full
    // explanation).
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        const string path = "/tmp/egt-icon-settings-person.svg";
        ofstream f(path); f << kPersonSvg; f.close();
        auto svg = std::make_shared<SvgImage>("file:" + path, SizeF(size, size));
        s_cache.push_back(svg);
        return static_cast<Image>(*svg);
    } catch (...) { return {}; }
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
    function<void()> on_wifi_settings,
    function<void()> on_login)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // The sections live inside a vertically-scrolling content frame so we can
    // append the "Account" section without crowding the fixed Back button.
    // The viewport stops just above Back (y=414); content taller than that
    // scrolls. With everything visible up front, the scrollbar only appears
    // once the Account card pushes past the fold.
    const int CONTENT_H = 540;             // tall enough for all sections
    auto content = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, CONTENT_H));
    content->fill_flags({Theme::FillFlag::blend});
    content->color(Palette::ColorId::bg, dt::kBgWhite);
    content->border(0);

    // Common section X (matches Figma left margin ≈ 81 px)
    // F1:1: Figma frame-local x=44 figma px * dt::SCALE = 81 device px.
    const int section_x = 81;

    // ── 1) Screen Brightness ────────────────────────────────────────────────
    // Title "Screen Brightness" — Figma (81, 31) 228x33  Gothic A1 Bold 14pt
    // -> device 26 pt.
    auto bright_title = make_shared<Label>("Screen Brightness",
        Rect(section_x, 31, 300, 33));
    bright_title->font(Font("Gothic A1", 26, Font::Weight::bold));
    bright_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    bright_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    content->add(bright_title);

    // Card "Rectangle 68" — Figma (78, 70) 628×87
    const int brt_card_x = 78;
    const int brt_card_y = 70;
    const int brt_card_w = 628;
    const int brt_card_h = 87;
    auto brt_card = make_section_card(brt_card_x, brt_card_y, brt_card_w, brt_card_h);
    content->add(brt_card);

    // Sun icons - PNG exports from Figma (left = thin "low brightness",
    // right = bold "high brightness"). Replaces the custom SunIcon widget
    // so the artwork is pixel-equivalent to the design.
    auto load_sun = [&](const std::string& png, int x, int y) {
        try {
            auto img = Image(("file:" + png).c_str());
            auto icon = make_shared<ImageLabel>(img);
            icon->autoresize(false);
            icon->border(0); icon->padding(0); icon->margin(0);
            icon->fill_flags({});
            icon->image_align(AlignFlag::center);
            icon->box(Rect(x, y, 28, 28));
            brt_card->add(icon);
        } catch (...) { /* fall back to no icon */ }
    };
    load_sun(ui::asset_path("settings-sun-left"),   53, 28);
    load_sun(ui::asset_path("settings-sun-right"), 535, 28);

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
    inet_title->font(Font("Gothic A1", 26, Font::Weight::bold));
    inet_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    inet_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    content->add(inet_title);

    // Cards: Wi-Fi at (81, 206) 306×113, Ethernet at (400, 206) 306×113
    const int chip_y = 206;
    const int chip_w = 306;
    const int chip_h = 113;
    const int wifi_x = 81;
    const int eth_x  = 400;

    auto add_internet_card = [&](int card_x, const string& title_text, bool is_wifi) {
        auto card = make_section_card(card_x, chip_y, chip_w, chip_h);
        content->add(card);

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

        // Glyph from Figma PNG (settings-wifi.png / settings-ethernet.png).
        // Custom Painter-drawn glyphs rendered as "briefcase-ish" shapes that
        // did not match the Figma art; the PNG exports are pixel-equivalent.
        const std::string icon_path = ui::asset_path(
            is_wifi ? "settings-wifi" : "settings-ethernet");
        try {
            auto img = Image(("file:" + icon_path).c_str());
            auto icon = make_shared<ImageLabel>(img);
            icon->autoresize(false);
            icon->border(0); icon->padding(0); icon->margin(0);
            icon->fill_flags({});
            icon->image_align(AlignFlag::center);
            // Natural device size (PNG_px * SCALE / 2):
            //   Wi-Fi: 56x42 PNG -> 52x39 device
            //   Eth:   53x50 PNG -> 49x46 device
            const int icon_w = is_wifi ? 52 : 49;
            const int icon_h = is_wifi ? 39 : 46;
            const int icon_dy = is_wifi ? 16 : 13;
            icon->box(Rect(circle_x + (circle_d - icon_w) / 2,
                           circle_y + icon_dy, icon_w, icon_h));
            card->add(icon);
        } catch (...) { /* fall back to no glyph */ }

        // Text - Figma "Wi-Fi" at card-relative (143, 44); fontSize 12 Bold
        // -> device 22 pt.
        auto lbl = make_shared<Label>(title_text,
            Rect(143, 0, chip_w - 143 - 16, chip_h));
        lbl->font(Font("Gothic A1", 22, Font::Weight::bold));
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

    // ── 3) Account ──────────────────────────────────────────────────────────
    // Same visual language as the Internet cards (gray circle + glyph + bold
    // label, full tap target), so it reads as part of the Settings grid rather
    // than a stray button. Tapping it returns to the Technician Login screen.
    if (on_login) {
        const int acc_title_y = 335;
        auto acc_title = make_shared<Label>("Account",
            Rect(section_x, acc_title_y, 320, 33));
        acc_title->font(Font("Gothic A1", 26, Font::Weight::bold));
        acc_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
        acc_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
        content->add(acc_title);

        // Full-width card (matches the brightness card geometry: x=78, w=628).
        const int acc_card_x = 78, acc_card_y = 371, acc_card_w = 628, acc_card_h = 84;
        auto acc_card = make_section_card(acc_card_x, acc_card_y, acc_card_w, acc_card_h);
        content->add(acc_card);

        // Gray circle + person glyph, vertically centred (mirrors Wi-Fi card).
        const int circle_d = 56;
        const int circle_x = 24;
        const int circle_y = (acc_card_h - circle_d) / 2;
        auto circle_bg = make_shared<Frame>(Rect(circle_x, circle_y, circle_d, circle_d));
        circle_bg->fill_flags({Theme::FillFlag::blend});
        circle_bg->color(Palette::ColorId::bg, palette::kGray200);
        circle_bg->border(0);
        circle_bg->border_radius(circle_d / 2);
        acc_card->add(circle_bg);

        auto person = load_person(34);
        if (!person.empty()) {
            const int isz = 34;
            auto pl = make_shared<ImageLabel>(person);
            pl->fill_flags({});
            pl->color(Palette::ColorId::bg, palette::kGray200);
            pl->image_align(AlignFlag::center);
            pl->move(Point(circle_x + (circle_d - isz) / 2,
                           circle_y + (circle_d - isz) / 2));
            pl->resize(Size(isz, isz));
            acc_card->add(pl);
        }

        auto acc_lbl = make_shared<Label>("Technician Login",
            Rect(circle_x + circle_d + 20, 0, acc_card_w - (circle_x + circle_d + 20) - 16, acc_card_h));
        acc_lbl->font(Font(15, Font::Weight::bold));
        acc_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        acc_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
        acc_card->add(acc_lbl);

        acc_card->on_event([on_login](Event&) { on_login(); }, {EventId::pointer_click});
    }

    // ── 4) About this device (last) ─────────────────────────────────────────
    // Two-column key/value list with everything we can read off the device.
    // Lives at the bottom of the scroll, so it never crowds the controls above.
    {
        const int about_title_y = (on_login ? 478 : 335);
        auto about_title = make_shared<Label>("About this device",
            Rect(section_x, about_title_y, 320, 33));
        about_title->font(Font("Gothic A1", 26, Font::Weight::bold));
        about_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
        about_title->text_align(AlignFlag::left | AlignFlag::center_vertical);
        content->add(about_title);

        const std::vector<std::pair<string, string>> rows = {
            {"Model",       get_model()},
            {"OS",          get_os_name()},
            {"Firmware",    "v" + get_firmware_version()},
            {"Serial #",    get_serial()},
            {"Kernel",      get_kernel()},
            {"Hostname",    get_hostname()},
            {"IP address",  get_ip_address()},
            {"MAC",         get_mac()},
            {"Wi-Fi SSID",  get_wifi_ssid()},
            {"Memory",      get_memory()},
            {"Storage",     get_storage()},
            {"Uptime",      get_uptime()},
            {"Date / time", get_datetime()},
        };

        const int row_h    = 27;
        const int key_w     = 150;
        const int val_x     = section_x + key_w;
        int y = about_title_y + 40;
        for (const auto& kv : rows) {
            auto k = make_shared<Label>(kv.first,
                Rect(section_x, y, key_w, row_h));
            k->font(Font(12, Font::Weight::bold));
            k->color(Palette::ColorId::label_text, dt::kTextPrimary);
            k->text_align(AlignFlag::left | AlignFlag::center_vertical);
            content->add(k);

            auto v = make_shared<Label>(kv.second,
                Rect(val_x, y, dt::SCREEN_W - val_x - 24, row_h));
            v->font(Font(12, Font::Weight::normal));
            v->color(Palette::ColorId::label_text, palette::kGray600);
            v->text_align(AlignFlag::left | AlignFlag::center_vertical);
            content->add(v);

            y += row_h;
        }

        // Grow the scroll content so the last row is fully reachable.
        const int needed = y + 16;
        if (needed > content->height())
            content->resize(Size(dt::SCREEN_W, needed));
    }

    // ── Scrollable viewport (scrollbar hidden) ──────────────────────────────
    // Stops just above the fixed Back button so Back stays put while the
    // sections scroll underneath. Horizontal scrolling is disabled (so the
    // brightness slider's horizontal drag isn't hijacked) and the vertical
    // scrollbar is painted transparent — see CleanScrolledView.
    auto scroll = make_shared<CleanScrolledView>(
        Rect(0, 0, dt::SCREEN_W, 410));
    scroll->add(content);
    scroll->offset(Point(0, 0));   // always open at the top (Screen Brightness)
    container->add(scroll);

    // ── Back button — shared layout via ui::add_back_button ────────────────
    // Same chevron-in-circle + label at the canonical bottom-left position
    // (Figma 2073:1996, 27,414). Fixed (outside the scroll) so it never drifts.
    ui::add_back_button(*container, on_back);

    return container;
}
