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
// Pixel-perfect refit using the spec extracted from /v1/files/{key}?depth=4.
// All dimensions are figma × dt::SCALE (=1.852).
//
// Element                       | Figma                | Device
// ------------------------------|----------------------|-----------------
// Frame bg (Rectangle 71)       | (0,0) 432×261        | (0,0) 800×484
// White card (Rectangle 72)     | (21,20) 390×219      | (39,37) 722×406
//   corner radius 8 → 15        | gradient #f4f3f3→#fff| shadow (4,4) r=8 a=.30
// Banner (Union 52:2778)        | (21,20) 390×61       | (39,37) 722×113
//   fill #ff9e1b                |                      |
// Banner icon (Group 146)       | (39,32) 36×36        | (72,59) 67×67
// Banner text (2071:1129)       | (117,34) 201×36      | (217,63) 372×67
//   Gothic A1 14@26 w500 BLACK  | align CENTER         |
// Operate card (Group 272)      | (68,90) 301×73       | (126,167) 557×135
//   Rectangle 7 white + r=15    | shadow r=10 a=.10    |
// Operate icon ellipse (gradient #d9d9d9 → #fff)        | 39×39 → 72×72
// Operate text (2071:1183)      | (142,99) 219×54      | (263,183) 406×100
//   Gothic A1 14@26 w700 #646569| line2-3 weight 400   |
// Retry card (Group 14 / 2071:1253) | (68,166) 133×52  | (126,307) 246×96
// Retry text (2071:1256)        | (117,184) 88×18      | (217,341) 163×33
//   Gothic A1 14@26 w700 #646569 CENTER align
// Setting card (Group 271)      | (220,166) 147×52     | (407,307) 272×96
// Setting text (2071:1197)      | (269,184) 68×18      | (498,341) 126×33

namespace {

// ── Vertical-gradient rounded card ────────────────────────────────────────
// Used for both the outer white card (Rectangle 72: #f4f3f3 → #ffffff) and
// any other element that needs a top-to-bottom linear gradient with rounded
// corners. EGT's plain Frame can only do solid fills.
class GradientCard : public Widget {
public:
    GradientCard(const Rect& r, const Color& top, const Color& bot,
                 float radius)
        : Widget(r), m_top(top), m_bot(bot), m_radius(radius) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float x = b.x(), y = b.y(), w = b.width(), h = b.height();
        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        // Rounded rect path (top-left arc → top edge → top-right arc → right edge
        //                    → bottom-right arc → bottom edge → bottom-left arc
        //                    → left edge → close)
        painter.draw(PointF(x + r, y));
        painter.line(PointF(x + w - r, y));
        painter.draw(Arc(PointF(x + w - r, y + r), r, -PI/2, 0.0f));
        painter.line(PointF(x + w, y + h - r));
        painter.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f, PI/2));
        painter.line(PointF(x + r, y + h));
        painter.draw(Arc(PointF(x + r, y + h - r), r, PI/2, PI));
        painter.line(PointF(x, y + r));
        painter.draw(Arc(PointF(x + r, y + r), r, PI, 3*PI/2));
        Pattern grad(Pattern::StepArray{{0.0f, m_top}, {1.0f, m_bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        painter.set(grad);
        painter.fill();
    }
private:
    Color m_top, m_bot;
    float m_radius;
};

// Banner with only the TOP corners rounded.
class TopRoundedBanner : public Widget {
public:
    TopRoundedBanner(const Rect& r, const Color& fill, float radius = 15.0f)
        : Widget(r), m_fill(fill), m_radius(radius) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float x = b.x(), y = b.y(), w = b.width(), h = b.height();
        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        painter.draw(PointF(x + r, y));
        painter.line(PointF(x + w - r, y));
        painter.draw(Arc(PointF(x + w - r, y + r), r, -PI/2, 0.0f));
        painter.line(PointF(x + w, y + h));
        painter.line(PointF(x, y + h));
        painter.line(PointF(x, y + r));
        painter.draw(Arc(PointF(x + r, y + r), r, PI, 3*PI/2));
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
  <path d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.74 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646569"/>
</svg>)svg";

static Image load_svg_icon(const char* name, const char* svg, int size) {
    try {
        string path = string("/tmp/egt-icon-nf-") + name + ".svg";
        ofstream f(path); f << svg; f.close();
        SvgImage img("file:" + path, SizeF(size, size));
        return static_cast<Image>(img);
    } catch (...) { return {}; }
}

// Gradient icon circle (#d9d9d9 → #ffffff) matching Figma's Ellipse 4.
// d = device diameter, glyph_sz = glyph rect side inside the circle.
shared_ptr<Widget> make_gradient_circle(int x, int y, int d) {
    return make_shared<GradientCard>(
        Rect(x, y, d, d), Color(0xd9, 0xd9, 0xd9), dt::kWhite, d / 2.0f);
}

shared_ptr<Frame> make_icon_circle_svg(int x, int y, int d,
                                       const char* name, const char* svg,
                                       int glyph_sz) {
    auto wrap = make_shared<Frame>(Rect(x, y, d, d));
    wrap->fill_flags({});  // transparent — GradientCard draws the disc
    wrap->add(make_gradient_circle(0, 0, d));
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
    auto wrap = make_shared<Frame>(Rect(x, y, d, d));
    wrap->fill_flags({});
    wrap->add(make_gradient_circle(0, 0, d));
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

// Glyph wrapper for the wifi-off Painter glyph centred inside a gradient
// circle (used by the Operate card's icon).
shared_ptr<Frame> make_wifi_off_circle(int x, int y, int d, const Color& glyph_col) {
    auto wrap = make_shared<Frame>(Rect(x, y, d, d));
    wrap->fill_flags({});
    wrap->add(make_gradient_circle(0, 0, d));
    wrap->add(make_shared<WifiOffGlyph>(Rect(0, 0, d, d), glyph_col));
    return wrap;
}

} // namespace

shared_ptr<Widget> create_wifi_not_found_screen(
    function<void()> on_operate_without_wifi,
    function<void()> on_retry_wifi,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, palette::kGray100);

    // ── Drop shadow under the white card (Figma: offset (4,4) r=8 a=0.30).
    // Emulate Gaussian blur with 3 stacked rects of decreasing alpha.
    const int card_x = 39, card_y = 37;
    const int card_w = 722, card_h = 406;
    for (const auto& s : {array<int,4>{6, 10, 6, 28},
                          array<int,4>{4,  6, 3, 50},
                          array<int,4>{4,  4, 1, 76}}) {
        auto sh = make_shared<Frame>(
            Rect(card_x + s[0] - s[2], card_y + s[1] - s[2],
                 card_w + 2 * s[2],    card_h + 2 * s[2]));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, s[3]));
        sh->border(0);
        sh->border_radius(15 + s[2]);
        container->add(sh);
    }

    // ── White card (Rectangle 72: gradient #f4f3f3 → #ffffff, r=15) ────────
    auto card = make_shared<GradientCard>(
        Rect(card_x, card_y, card_w, card_h),
        Color(0xf4, 0xf3, 0xf3), dt::kWhite, 15.0f);
    container->add(card);

    // Sub-card drop shadow helper (Figma: card Rectangle 7 has offset (0,0)
    // radius 10 alpha 0.10 — a soft glow more than a directional shadow).
    auto add_sub_shadow = [&](int x, int y, int w, int h, int radius) {
        for (const auto& s : {array<int,4>{0, 2, 5, 18},
                              array<int,4>{0, 1, 2, 26}}) {
            auto sh = make_shared<Frame>(
                Rect(x + s[0] - s[2], y + s[1] - s[2],
                     w + 2 * s[2],    h + 2 * s[2]));
            sh->fill_flags({Theme::FillFlag::blend});
            sh->color(Palette::ColorId::bg, Color(0, 0, 0, s[3]));
            sh->border(0);
            sh->border_radius(radius + s[2]);
            container->add(sh);
        }
    };

    // ── Banner (Union 52:2778: 722×113 at card_local (0, 0), fill #ff9e1b) ─
    const int banner_w = 722, banner_h = 113;
    container->add(make_shared<TopRoundedBanner>(
        Rect(card_x, card_y, banner_w, banner_h), Color(0xff, 0x9e, 0x1b),
        15.0f));

    // Banner icon (Group 146: 36×36 figma → 67×67 device at frame (39, 32)
    // → screen (72, 59)).
    const int b_icon_d = 67, b_icon_x = 72, b_icon_y = 59;
    auto b_chip = make_shared<Frame>(Rect(b_icon_x, b_icon_y, b_icon_d, b_icon_d));
    b_chip->fill_flags({Theme::FillFlag::blend});
    b_chip->color(Palette::ColorId::bg, dt::kWhite);
    b_chip->border(0);
    b_chip->border_radius(b_icon_d / 2);
    container->add(b_chip);
    // Glyph orange to match the banner (per design — looks like the strikethrough
    // ink "belongs" to the banner hue, not pure black).
    container->add(make_shared<WifiOffGlyph>(
        Rect(b_icon_x, b_icon_y, b_icon_d, b_icon_d), Color(0xff, 0x9e, 0x1b)));

    // Banner text (2071:1129: Gothic A1 14pt → 26pt, weight 500, #000000,
    // CENTER align). Position figma (117, 34) 201×36 → screen (217, 63).
    auto banner_txt = make_shared<Label>(
        "Wi-Fi Network not found.\nNo available network detected.",
        Rect(217 - 40, 63 - 6, 372 + 80, 67 + 12), AlignFlag::center);
    banner_txt->font(Font("Gothic A1", 22, Font::Weight::normal));
    banner_txt->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(banner_txt);

    // ── Operate without WiFi card (2073:1575: 557×135 at screen (126, 167)) ─
    const int op_x = 126, op_y = 167, op_w = 557, op_h = 135;
    add_sub_shadow(op_x, op_y, op_w, op_h, 15);
    auto op_card = make_shared<Frame>(Rect(op_x, op_y, op_w, op_h));
    op_card->fill_flags({Theme::FillFlag::blend});
    op_card->color(Palette::ColorId::bg, dt::kWhite);
    op_card->border(0);
    op_card->border_radius(15);
    container->add(op_card);

    // Operate icon: Group 248 → gradient circle + wifi-off glyph.
    // Figma frame (75, 109) 39×39 → screen (139, 202) 72×72.
    // Card-local: (139-126, 202-167) = (13, 35).
    const int op_ic_d = 72;
    op_card->add(make_wifi_off_circle(13, (op_h - op_ic_d) / 2, op_ic_d,
                                      dt::kTextPrimary));

    // Operate text — 3 visual lines from a single Figma TEXT node with
    // characterStyleOverrides (line 1 weight 700, lines 2-3 weight 400, all
    // colour #646569 = kTextPrimary, font 14pt → 26pt).
    // Figma bbox (142, 99) 219×54 → screen (263, 183) 406×100.
    // Card-local: (263-126, 183-167) = (137, 16).
    const int op_tx = 137, op_tw = op_w - op_tx - 14;
    auto t1 = make_shared<Label>("Operate without WiFi",
        Rect(op_tx, 14, op_tw, 32), AlignFlag::left | AlignFlag::center_vertical);
    t1->font(Font("Gothic A1", 24, Font::Weight::bold));
    t1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    op_card->add(t1);

    auto t2 = make_shared<Label>("Temporarily operate device in",
        Rect(op_tx, 52, op_tw, 28), AlignFlag::left | AlignFlag::center_vertical);
    t2->font(Font("Gothic A1", 19, Font::Weight::normal));
    t2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    op_card->add(t2);

    auto t3 = make_shared<Label>("OVERRIDE MODE.",
        Rect(op_tx, 84, op_tw, 30), AlignFlag::left | AlignFlag::center_vertical);
    // Per user preference: keep OVERRIDE MODE visually bolder than Figma's
    // weight 400 (the uppercase reads as emphasis but we lift it further).
    t3->font(Font("Gothic A1", 20, Font::Weight::bold));
    t3->color(Palette::ColorId::label_text, dt::kTextPrimary);
    op_card->add(t3);

    if (on_operate_without_wifi)
        op_card->on_event([on_operate_without_wifi](Event& e) {
            if (e.id() == EventId::pointer_click) on_operate_without_wifi();
        });

    // ── Retry WiFi card (Group 14 / 2071:1253: 246×96 at screen (126, 307)) ─
    const int row_y = 307;
    const int retry_x = 126, retry_w = 246, retry_h = 96;
    add_sub_shadow(retry_x, row_y, retry_w, retry_h, 15);
    auto retry_card = make_shared<Frame>(Rect(retry_x, row_y, retry_w, retry_h));
    retry_card->fill_flags({Theme::FillFlag::blend});
    retry_card->color(Palette::ColorId::bg, dt::kWhite);
    retry_card->border(0);
    retry_card->border_radius(15);
    container->add(retry_card);

    // Retry icon (Ellipse 4 at frame (75, 173) 39×39 → screen (139, 320) 72×72)
    // Card-local (139-126, 320-307) = (13, 13).
    const int r_ic_d = 72;
    auto retry_icon = make_icon_circle_svg(
        13, (retry_h - r_ic_d) / 2, r_ic_d, "refresh", kRefreshSvg, 38);
    retry_card->add(retry_icon);

    // "Retry WiFi" text (2071:1256: CENTER aligned in its 88×18 bbox at
    // frame (117, 184) → screen (217, 341)). Card-local (217-126, ...) = (91, 28).
    // Font 14pt → 26pt weight 700 #646569.
    auto retry_lbl = make_shared<Label>("Retry WiFi",
        Rect(91, 0, retry_w - 91 - 10, retry_h),
        AlignFlag::center_horizontal | AlignFlag::center_vertical);
    retry_lbl->font(Font("Gothic A1", 24, Font::Weight::bold));
    retry_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    retry_card->add(retry_lbl);

    if (on_retry_wifi)
        retry_card->on_event([on_retry_wifi](Event& e) {
            if (e.id() == EventId::pointer_click) on_retry_wifi();
        });

    // ── Setting card (Group 271: 272×96 at screen (407, 307)) ──────────────
    const int set_x = 407, set_w = 272, set_h = 96;
    add_sub_shadow(set_x, row_y, set_w, set_h, 15);
    auto set_card = make_shared<Frame>(Rect(set_x, row_y, set_w, set_h));
    set_card->fill_flags({Theme::FillFlag::blend});
    set_card->color(Palette::ColorId::bg, dt::kWhite);
    set_card->border(0);
    set_card->border_radius(15);
    container->add(set_card);

    // Setting icon (Ellipse 4 at frame (226, 173) → screen (419, 320)).
    // Card-local: (419-407, 320-307) = (12, 13).
    const int s_ic_d = 72;
    auto gear_icon = make_icon_circle_png(
        12, (set_h - s_ic_d) / 2, s_ic_d,
        "assets/figma/images/wifi-settings-gear.png", 48);
    set_card->add(gear_icon);

    // "Setting" text (2071:1197: CENTER aligned in its 68×18 bbox at
    // frame (269, 184) → screen (498, 341)). Card-local (498-407, ...) = (91, 28).
    auto set_lbl = make_shared<Label>("Setting",
        Rect(91, 0, set_w - 91 - 10, set_h),
        AlignFlag::center_horizontal | AlignFlag::center_vertical);
    set_lbl->font(Font("Gothic A1", 24, Font::Weight::bold));
    set_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    set_card->add(set_lbl);

    if (on_settings)
        set_card->on_event([on_settings](Event& e) {
            if (e.id() == EventId::pointer_click) on_settings();
        });

    return container;
}
