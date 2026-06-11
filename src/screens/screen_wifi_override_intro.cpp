#include "screen_wifi_override_intro.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

namespace {

// Same image-button helper used by screen_wifi_unavailable / screen_demo_info.
// A Frame sized to the PNG's natural device rect; the wrap captures clicks.
shared_ptr<Frame> make_image_button(
    const string& png_path,
    const Rect& rect,
    function<void()> on_click)
{
    auto wrap = make_shared<Frame>(rect);
    wrap->fill_flags({});

    try {
        auto probe = Image(("file:" + png_path).c_str());
        const float sw = static_cast<float>(probe.width());
        const float sh = static_cast<float>(probe.height());
        const float hs = static_cast<float>(rect.width())  / sw;
        const float vs = static_cast<float>(rect.height()) / sh;
        auto img = Image(("file:" + png_path).c_str(), hs, vs);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, rect.width(), rect.height()));
        wrap->add(lbl);
    } catch (const std::exception& e) {
        printf("[WIFI_OVR_INTRO] image %s missing: %s\n", png_path.c_str(), e.what());
        fflush(stdout);
    }

    wrap->on_event([on_click](Event& e) {
        if (e.id() == EventId::pointer_click && on_click) on_click();
    });

    return wrap;
}

// Build a rounded-rectangle path using draw(point)/line(point)/Arc so it works
// against both EGT 1.12 (host) and EGT 1.10 (target).
void rounded_path(Painter& p, float x, float y, float w, float h, float r)
{
    const auto PI = static_cast<float>(M_PI);
    p.draw(PointF(x + r,         y));
    p.line(PointF(x + w - r,     y));
    p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
    p.line(PointF(x + w,         y + h - r));
    p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,    PI / 2));
    p.line(PointF(x + r,         y + h));
    p.draw(Arc(PointF(x + r,     y + h - r),   r,  PI / 2, PI));
    p.line(PointF(x,             y + r));
    p.draw(Arc(PointF(x + r,     y + r),       r,  PI,     3 * PI / 2));
}

// ── Soft drop shadow with Gaussian-like falloff ─────────────────────────────
// Same 8-layer stacked-frame approximation as screen_wifi_not_found.cpp
// (commit 35ee2e9): alpha decays exponentially from the centre outward.
// Layers are plain Frames added BEFORE the button so the button (and the
// bottom row, added later) always paints and hit-tests on top of them.
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

// Cyan→blue gradient button with the "Enter Override password" label baked
// in. Replicates Figma 10:55 — vertical gradient #30a3c4 → #305fc4, corner
// radius 4 figma px (≈ 7 device). The drop shadow (offset(0,4) blur 4 alpha
// 0.20) is NOT painted here: an in-widget shadow gets clipped at the widget
// box, so it lives in soft-shadow frames added behind this widget instead.
// The text is painted by this widget so no child ImageLabel can swallow the
// click (same pattern as HOME's StartButton).
class OverridePasswordButton : public Widget {
public:
    OverridePasswordButton(const Rect& rect, function<void()> on_click)
        : Widget(rect), m_on_click(std::move(on_click)) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        on_event([this](Event& e) {
            switch (e.id()) {
                case EventId::raw_pointer_down: m_pressed = true;  damage(); break;
                case EventId::raw_pointer_up:   m_pressed = false; damage(); break;
                case EventId::pointer_click:    if (m_on_click) m_on_click(); break;
                default: break;
            }
        });
    }

    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = 7.0f;

        // Gradient body. Figma top stop is #30A3C4 (48,163,196); the shared
        // dt::kStartTop token is (48,154,196), so use a screen-local stop
        // here rather than editing the HOME-shared palette token.
        Color top = m_pressed ? dt::kStartTopPress : Color(48, 163, 196);
        Color bot = m_pressed ? dt::kStartBotPress : dt::kStartBottom;
        Pattern grad(Pattern::StepArray{{0.0f, top}, {1.0f, bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        rounded_path(painter, x, y, w, h, r);
        painter.set(grad);
        painter.fill();

        // Centred white label, painted in-widget so it never intercepts clicks.
        painter.set(Color(255, 255, 255));
        painter.set(Font("Gothic A1", 30, Font::Weight::bold));
        const std::string label = "Enter Override password";
        const auto ts = painter.text_size(label);
        painter.draw(PointF(x + (w - ts.width())  / 2.0f,
                            y + (h - ts.height()) / 2.0f));
        painter.draw(label);
    }

private:
    bool m_pressed{false};
    function<void()> m_on_click;
};

} // namespace

// ── Layout (Figma 134:1421 "S5-no3", 432×~263 → 800×~487 device) ───────────
// Frame-local Figma coordinates (after subtracting frame origin 1196, 2672):
//
//   Element                      Figma (x,y,w,h)        Device (x,y,w,h)
//   ───────────────────────────  ─────────────────────  ────────────────────
//   Orange banner Union          (0,   0,   432, 66)    (0,   0,   800, 122)
//   Wifi-off icon (Group 126)    (23,  15,  35.76²)     (43,  28,  66, 66)
//   Banner text (centred)        cx=236, y=28, w=336    cx=437, y=52, w=622
//   Body text (centred)          cx=216.5,y=79, w=301   cx=401, y=146,w=558
//   Cyan gradient button         (52,  134, 330, 66)    (96,  248, 611, 122)
//
// Figma node 134:1421 contains just the card portion; the bottom button row
// (Back / Retry WiFi / Setting) is mirrored from WiFi Unavailable so the user
// can bail out at any moment without backtracking through Continue.
shared_ptr<Widget> create_wifi_override_intro_screen(
    function<void()> on_enter_override,
    function<void()> on_back,
    function<void()> on_retry_wifi,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── 1) Orange banner ───────────────────────────────────────────────────
    const int banner_h = 122;
    auto banner = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    // Figma Union band samples flat #FF9E1B; dt::kOrange (palette::kWarning,
    // #FFA500) is shared app-wide, so override locally instead of editing it.
    banner->color(Palette::ColorId::bg, Color(255, 158, 27));
    banner->border(0);
    container->add(banner);

    // Wifi-off icon (same asset as WiFi Unavailable).
    auto banner_icon = make_image_button(
        ui::asset_path("wifi-banner-icon"),
        Rect(43, 28, 67, 67),
        nullptr);
    container->add(banner_icon);

    // Banner text — single line, centred vertically inside the banner.
    // Figma text box: w=336 (622 device), centred at x=437 device.
    auto banner_text = make_shared<Label>(
        "Wi-Fi/Network Connection remains unavailable",
        Rect(126, 0, 622, banner_h), AlignFlag::center);
    banner_text->font(Font("Gothic A1", 26, Font::Weight::normal));
    banner_text->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(banner_text);

    // ── 2) Body text ───────────────────────────────────────────────────────
    // Line breaks match the Figma render (3 lines, block at device y≈150-228).
    // Centring this 3-line block in the same rect lands its top at y≈149.
    auto body = make_shared<Label>(
        "a connection must be established, or an Override\n"
        "Password must be entered,\n"
        "in order to operate device.",
        Rect(60, 138, dt::SCREEN_W - 120, 100), AlignFlag::center);
    body->font(Font("Gothic A1", 22, Font::Weight::normal));
    body->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(body);

    // ── 3) Cyan→blue gradient "Enter Override password" button ─────────────
    // Figma 10:55: 330x66 at (52,134) → device (96,248,611,122); drop shadow
    // 0px 4px 4px rgba(0,0,0,0.2) → device offset (0,7), blur 7, peak 51.
    // Shadow frames sit behind the button (and behind the later-added bottom
    // row) so nothing clips it and the button hitbox stays exactly Figma-size.
    const Rect override_btn_rect(96, 248, 611, 122);
    add_soft_shadow(*container, override_btn_rect,
                    /*radius=*/7, /*off_x=*/0, /*off_y=*/7,
                    /*blur=*/7, /*alpha_peak=*/51);
    container->add(make_shared<OverridePasswordButton>(
        override_btn_rect, on_enter_override));

    // ── 4) Bottom button row (Back / Retry WiFi / Setting) ─────────────────
    // Same PNG assets + positions as screen_wifi_unavailable so the bottom
    // chrome stays consistent across the WiFi failure subtree.
    container->add(make_image_button(
        ui::asset_path("wifi-back-btn"),    Rect(7,   374, 241, 91), on_back));
    container->add(make_image_button(
        ui::asset_path("wifi-retry-btn"),   Rect(293, 374, 241, 91), on_retry_wifi));
    container->add(make_image_button(
        ui::asset_path("wifi-setting-btn"), Rect(579, 374, 210, 91), on_settings));

    return container;
}
