#include "screen_wifi_not_found.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <egt/svgimage.h>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

// ── Figma source: file PBh3UuMmYwdFzalAyPTMBw, M4-19 "Wi-Fi Network not found"
// Frame 432×261 figma. Rendered as a white rounded card with visible gray
// margin around it + drop shadow (matches the Figma editor render the
// design team showed). Card_w = 800-48 = 752, card_h = 480-40 = 440 →
// effective scale within card = 752/432 ≈ 1.741.
//
// All inner positions below are CARD-LOCAL, scaled from Figma frame coords:
//   card_local = figma * (card_w / 432)
//
// Element                       | Figma         | Card-local (×1.741)
// ------------------------------|---------------|-----------------------
// Banner (Union 52:2778)        | (21,20) 390×61| (37,34)   679×103
// Banner icon  (Group 146)      | (39,32) 36×36 | (68,54)   62×62
// Banner text  (2071:1129)      | (117,34)201×36| (204,57)  350×61
// Operate card (Group 272)      | (68,90) 301×73| (118,152) 524×123
// Operate icon (Group 248)      | (75,109) 39×39| (131,184) 67×67
// Operate text (2071:1183)      | (142,99)219×54| (247,167) 381×91
// Setting card (Group 271)      | (220,166)147x52|(383,280) 256×88
// Setting icon (Ellipse 4)      | (226,173) 39×39|(393,292) 67×67
// Setting text (2071:1197)      | (269,184) 68×18|(468,310) 118×30

namespace {

// Banner with only the TOP corners rounded.
class TopRoundedBanner : public Widget {
public:
    TopRoundedBanner(const Rect& r, const Color& fill, float radius = 12.0f)
        : Widget(r), m_fill(fill), m_radius(radius) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        painter.draw(PointF(x + r, y));
        painter.line(PointF(x + w - r, y));
        painter.draw(Arc(PointF(x + w - r, y + r), r, -PI / 2, 0.0f));
        painter.line(PointF(x + w, y + h));
        painter.line(PointF(x, y + h));
        painter.line(PointF(x, y + r));
        painter.draw(Arc(PointF(x + r, y + r), r, PI, 3 * PI / 2));
        painter.set(m_fill);
        painter.fill();
    }
private:
    Color m_fill;
    float m_radius;
};

class WifiOffGlyph : public Widget {
public:
    WifiOffGlyph(const Rect& rect, const Color& col) : Widget(rect), m_col(col) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        constexpr float start = -static_cast<float>(M_PI) * 0.75f;
        constexpr float end   = -static_cast<float>(M_PI) * 0.25f;
        const auto pivot = PointF(cx, cy + sz * 0.18f);
        const float radii[] = {sz * 0.34f, sz * 0.24f, sz * 0.13f};
        painter.set(m_col);
        painter.line_width(std::max(2.0f, sz * 0.055f));
        for (float r : radii) { painter.draw(Arc(pivot, r, start, end)); painter.stroke(); }
        painter.draw(Arc(pivot, sz * 0.05f, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
        painter.line_width(std::max(2.5f, sz * 0.065f));
        painter.draw(Line(PointF(cx - sz * 0.32f, cy - sz * 0.23f),
                          PointF(cx + sz * 0.32f, cy + sz * 0.23f)));
        painter.stroke();
    }
private:
    Color m_col;
};

static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.74 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#28282d"/>
</svg>)svg";

static Image load_svg_icon(const char* name, const char* svg, int size) {
    try {
        string path = string("/tmp/egt-icon-nf-") + name + ".svg";
        ofstream f(path); f << svg; f.close();
        SvgImage img("file:" + path, SizeF(size, size));
        return static_cast<Image>(img);
    } catch (...) { return {}; }
}

shared_ptr<Frame> make_icon_circle(int x, int y, int d) {
    auto wrap = make_shared<Frame>(Rect(x, y, d, d));
    wrap->fill_flags({Theme::FillFlag::blend});
    wrap->color(Palette::ColorId::bg, palette::kGray200);
    wrap->border(0);
    wrap->border_radius(d / 2);
    return wrap;
}

shared_ptr<Frame> make_icon_circle_svg(int x, int y, int d,
                                       const char* name, const char* svg,
                                       int glyph_sz) {
    auto wrap = make_icon_circle(x, y, d);
    auto img = load_svg_icon(name, svg, glyph_sz);
    if (!img.empty()) {
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect((d - glyph_sz) / 2, (d - glyph_sz) / 2, glyph_sz, glyph_sz));
        wrap->add(lbl);
    }
    return wrap;
}

shared_ptr<Frame> make_icon_circle_png(int x, int y, int d,
                                       const string& png_path, int glyph_sz) {
    auto wrap = make_icon_circle(x, y, d);
    try {
        auto probe = Image(("file:" + png_path).c_str());
        const float s = static_cast<float>(glyph_sz)
                      / static_cast<float>(max(probe.width(), probe.height()));
        auto img = Image(("file:" + png_path).c_str(), s, s);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect((d - glyph_sz) / 2, (d - glyph_sz) / 2, glyph_sz, glyph_sz));
        wrap->add(lbl);
    } catch (...) {}
    return wrap;
}

// Slightly warmer / muted dark gray (less near-black, more "design-y").
inline Color kInk()      { return Color(70, 70, 78); }
inline Color kInkLight() { return palette::kGray600; }

} // namespace

shared_ptr<Widget> create_wifi_not_found_screen(
    function<void()> on_operate_without_wifi,
    function<void()> on_retry_wifi,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, palette::kGray100);

    // ── Diffuse drop shadow under the white card. EGT has no native blur;
    // approximate Gaussian-feathered edges with 3 stacked rectangles of
    // decreasing alpha and growing offsets/sizes — softer than a single
    // hard-offset rect.
    const int card_x = 24, card_y = 20;
    const int card_w = dt::SCREEN_W - 2 * card_x;          // 752
    const int card_h = dt::SCREEN_H - 2 * card_y;          // 440
    struct ShadowLayer { int dx, dy, grow, alpha; };
    for (const auto& s : {ShadowLayer{8, 12, 8, 24},
                          ShadowLayer{5, 8, 4,  36},
                          ShadowLayer{3, 5, 2,  48}}) {
        auto sh = make_shared<Frame>(
            Rect(card_x + s.dx - s.grow, card_y + s.dy - s.grow,
                 card_w + 2 * s.grow,    card_h + 2 * s.grow));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, s.alpha));
        sh->border(0);
        sh->border_radius(18 + s.grow);
        container->add(sh);
    }

    // ── White card englobante ──────────────────────────────────────────────
    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->border(0);
    card->border_radius(18);
    container->add(card);

    // Sub-card shadow helper — 2-layer feathered shadow (less heavy than the
    // root card's 3-layer one, just enough to lift each sub-card).
    auto add_sub_shadow = [&](int x, int y, int w, int h, int radius) {
        for (const auto& s : {std::array<int,4>{4, 6, 4, 14},
                              std::array<int,4>{2, 3, 2, 22}}) {
            auto sh = make_shared<Frame>(
                Rect(x + s[0] - s[2], y + s[1] - s[2],
                     w + 2 * s[2],    h + 2 * s[2]));
            sh->fill_flags({Theme::FillFlag::blend});
            sh->color(Palette::ColorId::bg, Color(0, 0, 0, s[3]));
            sh->border(0);
            sh->border_radius(radius + s[2]);
            card->add(sh);
        }
    };

    // ── Banner ─────────────────────────────────────────────────────────────
    // Figma (21,20) 390×61 → card-local (37,34) 679×103
    const int banner_x = 37, banner_y = 34, banner_w = 679, banner_h = 103;
    auto banner = make_shared<TopRoundedBanner>(
        Rect(banner_x, banner_y, banner_w, banner_h), dt::kOrange, 18.0f);
    card->add(banner);

    // Banner icon: white disc + dark wifi-off glyph
    // Figma (39,32) 36×36 → card-local (68,54) 63×63
    const int b_icon_d = 62, b_icon_x = 68, b_icon_y = 54;
    auto b_chip = make_shared<Frame>(Rect(b_icon_x, b_icon_y, b_icon_d, b_icon_d));
    b_chip->fill_flags({Theme::FillFlag::blend});
    b_chip->color(Palette::ColorId::bg, dt::kWhite);
    b_chip->border(0);
    b_chip->border_radius(b_icon_d / 2);
    card->add(b_chip);
    // Banner glyph rendered in orange (matches Figma — the slash crosses the
    // wifi arcs in the same hue as the banner so the icon "belongs" to it).
    card->add(make_shared<WifiOffGlyph>(
        Rect(b_icon_x, b_icon_y, b_icon_d, b_icon_d), dt::kOrange));

    // Banner text: Figma (117,34) 201×36 → card-local (204,57) 350×61.
    // NORMAL weight (not bold) per Figma typography.
    auto banner_txt = make_shared<Label>(
        "Wi-Fi Network not found.\nNo available network detected.",
        Rect(204, 57, 350 + 80, 61), AlignFlag::center);
    banner_txt->font(Font("Gothic A1", 22, Font::Weight::normal));
    banner_txt->color(Palette::ColorId::label_text, Color(50, 50, 55));
    card->add(banner_txt);

    // ── Operate without WiFi card ──────────────────────────────────────────
    // Figma (68,90) 301×73 → card-local (118,152) 524×123
    const int op_x = 118, op_y = 152, op_w = 524, op_h = 123;
    add_sub_shadow(op_x, op_y, op_w, op_h, 12);
    auto op_card = make_shared<Frame>(Rect(op_x, op_y, op_w, op_h));
    op_card->fill_flags({Theme::FillFlag::blend});
    op_card->color(Palette::ColorId::bg, palette::kGray50);
    op_card->border(0);
    op_card->border_radius(12);
    card->add(op_card);

    // Operate icon: Figma (75,109) 39×39 → card-local (131,184) 68×68 → 67
    // op_card local: 131-118=13, 184-152=32, size 67×67
    const int op_ic_d = 67;
    auto op_icon = make_icon_circle(13, (op_h - op_ic_d) / 2, op_ic_d);
    op_icon->add(make_shared<WifiOffGlyph>(
        Rect(0, 0, op_ic_d, op_ic_d), Color(40, 40, 45)));
    op_card->add(op_icon);

    // Operate text: 3 lines
    // op_card local x for text: 247-118 = 129
    const int op_tx = 129, op_tw = op_w - op_tx - 14;
    auto t1 = make_shared<Label>("Operate without WiFi",
        Rect(op_tx, 14, op_tw, 32), AlignFlag::left | AlignFlag::center_vertical);
    t1->font(Font("Gothic A1", 24, Font::Weight::bold));
    t1->color(Palette::ColorId::label_text, kInk());
    op_card->add(t1);

    auto t2 = make_shared<Label>("Temporarily operate device in",
        Rect(op_tx, 50, op_tw, 26), AlignFlag::left | AlignFlag::center_vertical);
    t2->font(Font("Gothic A1", 18, Font::Weight::normal));
    t2->color(Palette::ColorId::label_text, kInkLight());
    op_card->add(t2);

    auto t3 = make_shared<Label>("OVERRIDE MODE.",
        Rect(op_tx, 80, op_tw, 28), AlignFlag::left | AlignFlag::center_vertical);
    t3->font(Font("Gothic A1", 20, Font::Weight::bold));
    t3->color(Palette::ColorId::label_text, kInk());
    op_card->add(t3);

    if (on_operate_without_wifi)
        op_card->on_event([on_operate_without_wifi](Event& e) {
            if (e.id() == EventId::pointer_click) on_operate_without_wifi();
        });

    // ── Setting + Retry WiFi (half-width row) ──────────────────────────────
    // Per user feedback: Retry / Setting cards should have the SAME HEIGHT
    // as the Operate card (visually balanced). They stay half-width to keep
    // the side-by-side row layout from Figma.
    const int set_w = 256, set_h = op_h;   // same height as Operate
    const int set_y = op_y + op_h + 14;
    (void)0;  // documentation breakpoint
    // Setting Figma (220,166) 147×52 → card-local (383,280) but with the new
    // taller card we vertically shift it down slightly to keep spacing tight.
    const int set_right_x = 383;
    const int set_left_x  = card_w - set_right_x - set_w;     // mirror

    // Icons grow with the taller card (proportional to card height).
    // Tight padding so "Retry WiFi" fits without clipping.
    const int s_ic_d = 72;
    const int s_ic_x = 14;
    const int s_ic_y = (set_h - s_ic_d) / 2;
    const int s_tx = s_ic_x + s_ic_d + 10;
    const int s_tw = set_w - s_tx - 10;

    // Retry WiFi (left)
    add_sub_shadow(set_left_x, set_y, set_w, set_h, 12);
    auto retry_card = make_shared<Frame>(Rect(set_left_x, set_y, set_w, set_h));
    retry_card->fill_flags({Theme::FillFlag::blend});
    retry_card->color(Palette::ColorId::bg, palette::kGray50);
    retry_card->border(0);
    retry_card->border_radius(12);
    card->add(retry_card);

    auto retry_icon = make_icon_circle_svg(
        s_ic_x, s_ic_y, s_ic_d, "refresh", kRefreshSvg, 54);
    retry_card->add(retry_icon);

    auto retry_lbl = make_shared<Label>("Retry WiFi",
        Rect(s_tx, 0, s_tw, set_h),
        AlignFlag::left | AlignFlag::center_vertical);
    retry_lbl->font(Font("Gothic A1", 24, Font::Weight::bold));
    retry_lbl->color(Palette::ColorId::label_text, kInk());
    retry_card->add(retry_lbl);

    if (on_retry_wifi)
        retry_card->on_event([on_retry_wifi](Event& e) {
            if (e.id() == EventId::pointer_click) on_retry_wifi();
        });

    // Setting (right)
    add_sub_shadow(set_right_x, set_y, set_w, set_h, 12);
    auto set_card = make_shared<Frame>(Rect(set_right_x, set_y, set_w, set_h));
    set_card->fill_flags({Theme::FillFlag::blend});
    set_card->color(Palette::ColorId::bg, palette::kGray50);
    set_card->border(0);
    set_card->border_radius(12);
    card->add(set_card);

    auto gear_icon = make_icon_circle_png(
        s_ic_x, s_ic_y, s_ic_d,
        "assets/figma/images/wifi-settings-gear.png", 60);
    set_card->add(gear_icon);

    auto set_lbl = make_shared<Label>("Setting",
        Rect(s_tx, 0, s_tw, set_h),
        AlignFlag::left | AlignFlag::center_vertical);
    set_lbl->font(Font("Gothic A1", 24, Font::Weight::bold));
    set_lbl->color(Palette::ColorId::label_text, kInk());
    set_card->add(set_lbl);

    if (on_settings)
        set_card->on_event([on_settings](Event& e) {
            if (e.id() == EventId::pointer_click) on_settings();
        });

    return container;
}
