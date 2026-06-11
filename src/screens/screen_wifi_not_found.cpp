#include "screen_wifi_not_found.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <egt/svgimage.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

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

// Figma wifi-off glyph (52:2785 "elements", viewBox 27.6269 × 25.3656).
// Verbatim path data from Figma's SVG export — 5 asymmetric stroked arcs,
// the small bottom-centre ellipse and the diagonal slash. The "%C" tokens
// are placeholders for the stroke colour so we can render one variant per
// (colour, size) on demand. Source: REST/MCP export of node 52:2785.
static const char* kWifiOffGlyphFigmaSvg = R"svg(<svg viewBox="0 0 27.6269 25.3656" xmlns="http://www.w3.org/2000/svg" fill="none">
<path d="M9.32881 15.6035C10.7657 14.3119 12.4864 13.7191 14.4542 13.8759" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M22.1422 12.0987C20.2296 10.5559 18.0224 9.49848 15.7355 9.17814" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M5.48467 12.0985C6.88517 11.0344 8.40357 10.2445 9.96936 9.76196" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M26.6268 8.59373C21.5934 4.71837 16.0381 3.25341 10.6101 4.19888" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M1.00005 8.59387C2.56853 7.38625 4.2034 6.25731 5.48474 5.67317" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<ellipse cx="13.8134" cy="19.6925" rx="1.92201" ry="1.75242" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M1.00011 1.00001L26.6269 24.3656" stroke="%C" stroke-width="2" stroke-linecap="round"/>
</svg>)svg";

// Cache wifi-off SvgImage instances by (RGB, size). Keeps the SvgImage
// alive so the sliced Image's rasterized buffer stays valid on EGT 1.10
// (same hazard as load_svg_icon — see comments there).
static Image get_wifi_off_image(const Color& stroke, int size_px)
{
    using Key = std::tuple<uint8_t, uint8_t, uint8_t, int>;
    static std::map<Key, std::shared_ptr<SvgImage>> s_cache;
    Key k{stroke.red(), stroke.green(), stroke.blue(), size_px};
    auto it = s_cache.find(k);
    if (it != s_cache.end()) return static_cast<Image>(*it->second);

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x",
             stroke.red(), stroke.green(), stroke.blue());
    string svg = kWifiOffGlyphFigmaSvg;
    string::size_type pos = 0;
    while ((pos = svg.find("%C", pos)) != string::npos)
        svg.replace(pos, 2, hex);

    char fname[80];
    snprintf(fname, sizeof(fname),
             "/tmp/egt-icon-wifioff-%02x%02x%02x-%d.svg",
             stroke.red(), stroke.green(), stroke.blue(), size_px);
    ofstream f(fname); f << svg; f.close();
    try {
        auto img = std::make_shared<SvgImage>(string("file:") + fname,
                                              SizeF(size_px, size_px));
        s_cache.emplace(k, img);
        return static_cast<Image>(*img);
    } catch (...) {
        return {};
    }
}

// Build a Frame holding a centred ImageLabel that renders the wifi-off
// glyph SVG at exactly `glyph_sz` device px tall, stroked in `col`.
// Versioned painter image draws (Painter::draw(Rect, Image)) diverge
// between EGT 1.10 (target) and 1.12 (host), so a child ImageLabel is the
// portable way — same trick we use elsewhere.
static shared_ptr<Frame>
make_wifi_off_glyph_frame(const Rect& outer, const Color& col)
{
    auto wrap = make_shared<Frame>(outer);
    wrap->fill_flags({});
    wrap->border(0);
    const int d = min(outer.width(), outer.height());
    const int gs = static_cast<int>(d * 0.65f);  // glyph fills ~65% of disc
    auto img = get_wifi_off_image(col, gs);
    if (!img.empty()) {
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect((outer.width() - gs) / 2,
                      (outer.height() - gs) / 2, gs, gs));
        wrap->add(lbl);
    }
    return wrap;
}

static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.74 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646569"/>
</svg>)svg";

static Image load_svg_icon(const char* name, const char* svg, int size) {
    // libegt 1.10 (target) heap-corruption fix: keep the SvgImage alive in
    // a static cache so the returned sliced-Image's backing buffer stays
    // valid (see screen_patient_info.cpp load_svg_icon).
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        string path = string("/tmp/egt-icon-nf-") + name + ".svg";
        ofstream f(path); f << svg; f.close();
        auto img = std::make_shared<SvgImage>("file:" + path, SizeF(size, size));
        s_cache.push_back(img);
        return static_cast<Image>(*img);
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

// Glyph wrapper: gradient disc + Figma wifi-off SVG glyph centred on it.
shared_ptr<Frame> make_wifi_off_circle(int x, int y, int d, const Color& glyph_col) {
    auto wrap = make_shared<Frame>(Rect(x, y, d, d));
    wrap->fill_flags({});
    wrap->add(make_gradient_circle(0, 0, d));
    wrap->add(make_wifi_off_glyph_frame(Rect(0, 0, d, d), glyph_col));
    return wrap;
}

// ── Soft drop shadow with Gaussian-like falloff ───────────────────────────
// EGT has no real blur; we stack N translucent rounded rects with alpha
// that decays exponentially from the centre, producing a closer match to
// Figma's CSS box-shadow than the previous 2/3-layer constant-alpha stack.
//
//   centre_rect    — the rect the shadow sits BEHIND (where the card lives)
//   radius         — corner radius of the card so shadow rounds match
//   off_x/off_y    — Figma boxShadow offset (device px)
//   blur_radius    — Figma blur radius (device px) — half-width of the falloff
//   alpha_peak     — 0..255 alpha at the centre of the shadow
//
// Layers double-fill the inner region so the alpha there sums to roughly
// alpha_peak; the alpha at the outer edge tapers to ~3% of peak (Gaussian
// tail ≈ exp(-3.5)).
static void add_soft_shadow(Frame& parent, const Rect& centre_rect,
                            int radius, int off_x, int off_y,
                            int blur_radius, int alpha_peak)
{
    constexpr int LAYERS = 8;
    const int cx = centre_rect.x() + off_x;
    const int cy = centre_rect.y() + off_y;
    const int cw = centre_rect.width();
    const int ch = centre_rect.height();
    for (int i = 0; i < LAYERS; i++) {
        // t in [0,1]: 0 at the outermost (faintest) layer, 1 at the centre.
        const float t = static_cast<float>(LAYERS - 1 - i)
                      / static_cast<float>(LAYERS - 1);
        const int grow = static_cast<int>(blur_radius * (1.0f - t));
        // Gaussian-like envelope at radius t·blur. Peak (t=1) hits ~1.0.
        const float env = std::exp(-(1.0f - t) * (1.0f - t) * 3.5f);
        const int a = std::max(1,
            static_cast<int>(alpha_peak * env / static_cast<float>(LAYERS)));
        auto sh = make_shared<Frame>(
            Rect(cx - grow, cy - grow, cw + 2 * grow, ch + 2 * grow));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, a));
        sh->border(0);
        sh->border_radius(radius + grow);
        parent.add(sh);
    }
}

// ── PressableOperateCard ──────────────────────────────────────────────────
// Custom widget for the "Operate without WiFi" card that swaps to the
// Figma 2073:1617 pressed-state visual while a finger is on it:
//   default   — white bg, gray text/glyph, gray→white icon disc
//   pressed   — cyan(top)→blue(bottom) gradient bg, white text/glyph
//
// All painting happens in draw() so no overlaid child Label/ImageLabel can
// intercept the click on EGT 1.10 (same z-order trap we hit in HOME's Start
// button). on_click fires once on release inside the rect.
class PressableOperateCard : public Widget {
public:
    PressableOperateCard(const Rect& r, function<void()> on_click)
        : Widget(r), m_on_click(std::move(on_click))
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        on_event([this](Event& e) {
            switch (e.id()) {
                case EventId::raw_pointer_down:
                    if (!m_pressed) { m_pressed = true;  damage(); }
                    break;
                case EventId::raw_pointer_up:
                    if (m_pressed)  { m_pressed = false; damage(); }
                    break;
                case EventId::pointer_click:
                    if (m_on_click) m_on_click();
                    break;
                default: break;
            }
        });
    }

    void draw(Painter& painter, const Rect&) override
    {
        const auto b = content_area();
        const float x = b.x(), y = b.y(), w = b.width(), h = b.height();
        const float r = 15.0f;

        // ── Background: rounded rect, gradient if pressed, white otherwise.
        const auto PI = static_cast<float>(M_PI);
        painter.draw(PointF(x + r, y));
        painter.line(PointF(x + w - r, y));
        painter.draw(Arc(PointF(x + w - r, y + r), r, -PI/2, 0.0f));
        painter.line(PointF(x + w, y + h - r));
        painter.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f, PI/2));
        painter.line(PointF(x + r, y + h));
        painter.draw(Arc(PointF(x + r, y + h - r), r, PI/2, PI));
        painter.line(PointF(x, y + r));
        painter.draw(Arc(PointF(x + r, y + r), r, PI, 3*PI/2));
        if (m_pressed) {
            Pattern grad(Pattern::StepArray{
                {0.0f, Color(0x30, 0xa3, 0xc4)},
                {1.0f, Color(0x30, 0x5f, 0xc4)}},
                Point(static_cast<int>(x), static_cast<int>(y)),
                Point(static_cast<int>(x), static_cast<int>(y + h)));
            painter.set(grad);
        } else {
            painter.set(dt::kWhite);
        }
        painter.fill();

        // ── Icon disc (gradient circle) at left ──────────────────────────
        const float icon_d = 72.0f;
        const float icon_x = x + 13.0f;
        const float icon_y = y + (h - icon_d) / 2.0f;
        const float icon_r = icon_d / 2.0f;
        const float icon_cx = icon_x + icon_r;
        const float icon_cy = icon_y + icon_r;

        // Disc gradient: #d9d9d9 → #ffffff vertical.
        painter.draw(Arc(PointF(icon_cx, icon_cy), icon_r, 0.0f, 2.0f * PI));
        Pattern disc_grad(Pattern::StepArray{
            {0.0f, Color(0xd9, 0xd9, 0xd9)}, {1.0f, dt::kWhite}},
            Point(static_cast<int>(icon_x), static_cast<int>(icon_y)),
            Point(static_cast<int>(icon_x), static_cast<int>(icon_y + icon_d)));
        painter.set(disc_grad);
        painter.fill();

        // WiFi-off glyph: the asymmetric Figma SVG (52:2785 "elements")
        // rasterised at the exact device size and pasted via the painter.
        // Stays gray (#646569) in BOTH default and pressed states — Figma's
        // 2073:1620 (pressed) reuses the same stroke colour as 2073:1286,
        // even though the surrounding card swaps to the cyan→blue gradient.
        const int gs = static_cast<int>(icon_d * 0.65f);
        const int gx = static_cast<int>(icon_cx - gs / 2.0f);
        const int gy = static_cast<int>(icon_cy - gs / 2.0f);
        auto glyph = get_wifi_off_image(dt::kTextPrimary, gs);
        if (!glyph.empty()) {
            // Point-then-image idiom: portable across EGT 1.10 (target) and
            // 1.12 (host) — neither has draw(Rect, Image).
            painter.draw(PointF(static_cast<float>(gx), static_cast<float>(gy)));
            painter.draw(glyph);
        }

        // ── Text block — three labels stacked. Colours flip on press. ────
        const Color text_col = m_pressed ? dt::kWhite : dt::kTextPrimary;
        const float text_x = x + 137.0f;

        painter.set(text_col);
        painter.set(Font("Gothic A1", 24, Font::Weight::bold));
        const std::string line1 = "Operate without WiFi";
        const auto ts1 = painter.text_size(line1);
        painter.draw(PointF(text_x, y + 14.0f + (32.0f - ts1.height()) / 2.0f));
        painter.draw(line1);

        painter.set(Font("Gothic A1", 19, Font::Weight::normal));
        const std::string line2 = "Temporarily operate device in";
        const auto ts2 = painter.text_size(line2);
        painter.draw(PointF(text_x, y + 52.0f + (28.0f - ts2.height()) / 2.0f));
        painter.draw(line2);

        painter.set(Font("Gothic A1", 20, Font::Weight::bold));
        const std::string line3 = "OVERRIDE MODE.";
        const auto ts3 = painter.text_size(line3);
        painter.draw(PointF(text_x, y + 84.0f + (30.0f - ts3.height()) / 2.0f));
        painter.draw(line3);
    }

private:
    bool m_pressed{false};
    function<void()> m_on_click;
};

} // namespace

shared_ptr<Widget> create_wifi_not_found_screen(
    function<void()> on_operate_without_wifi,
    function<void()> on_retry_wifi,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, palette::kGray100);

    // ── Drop shadow under the white card (Figma 52:2775: offset (4,4),
    // blur 8 figma → 15 device, alpha 0.30 → 76/255). Soft-shadow helper
    // does the Gaussian-falloff approximation with 8 stacked layers.
    const int card_x = 39, card_y = 37;
    const int card_w = 722, card_h = 406;
    // Figma 52:2775 box-shadow is rgba(0,0,0,0.30) → peak 76/255. The old
    // 180 read ~3x too dark with visible banding below/right of the card.
    add_soft_shadow(*container, Rect(card_x, card_y, card_w, card_h),
                    15, /*off_x=*/7, /*off_y=*/7,
                    /*blur=*/15, /*alpha_peak=*/76);

    // ── White card (Rectangle 72: gradient #f4f3f3 → #ffffff, r=15) ────────
    auto card = make_shared<GradientCard>(
        Rect(card_x, card_y, card_w, card_h),
        Color(0xf4, 0xf3, 0xf3), dt::kWhite, 15.0f);
    container->add(card);

    // Sub-card drop shadow helper (Figma 2073:1575: offset (0,0), blur 10
    // figma → 18 device, alpha 0.10 → 26/255). Lighter, omnidirectional
    // halo around each tappable card.
    auto add_sub_shadow = [&](int x, int y, int w, int h, int radius) {
        add_soft_shadow(*container, Rect(x, y, w, h),
                        radius, /*off_x=*/0, /*off_y=*/0,
                        /*blur=*/18, /*alpha_peak=*/60);
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
    // ink "belongs" to the banner hue, not pure black). Figma SVG paths
    // rendered by make_wifi_off_glyph_frame() match the actual Group 146 art.
    container->add(make_wifi_off_glyph_frame(
        Rect(b_icon_x, b_icon_y, b_icon_d, b_icon_d), Color(0xff, 0x9e, 0x1b)));

    // Banner text (2071:1129: Gothic A1 14pt → 26pt, weight 500, #000000,
    // CENTER align). Position figma (117, 34) 201×36 → screen (217, 63).
    auto banner_txt = make_shared<Label>(
        "Wi-Fi Network not found.\nNo available network detected.",
        Rect(217 - 40, 63 - 6, 372 + 80, 67 + 12), AlignFlag::center);
    banner_txt->font(Font("Gothic A1", 22, Font::Weight::normal));
    banner_txt->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(banner_txt);

    // ── Operate without WiFi card (2073:1575 default / 2073:1617 pressed)
    // Single Painter-drawn widget so the press-state flip (white card →
    // cyan→blue gradient + white text + white glyph, per Figma 2073:1618)
    // works on EGT 1.10 without child-widget z-order traps. Fires on_click
    // on pointer_click as usual.
    const int op_x = 126, op_y = 167, op_w = 557, op_h = 135;
    add_sub_shadow(op_x, op_y, op_w, op_h, 15);
    container->add(make_shared<PressableOperateCard>(
        Rect(op_x, op_y, op_w, op_h),
        on_operate_without_wifi));

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
        ui::asset_path("wifi-settings-gear"), 48);
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
