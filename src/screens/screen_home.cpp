#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_home.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <cmath>
#include <fstream>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

// ── SVG icon helpers ─────────────────────────────────────────────────────────
// Two glyphs sit on the bottom-left ("Demo Mode") and bottom-right ("Setting")
// cards, each inside a 56-px gray circle (Figma "Group 263"). The SVG fills
// use the primary text colour so they read crisply on the light gray disc.
static string write_svg_tmp(const char* name, const char* svg_data)
{
    string path = string("/tmp/egt-icon-") + name + ".svg";
    static bool written_person = false;
    static bool written_gear   = false;
    bool& written = (string(name) == "person") ? written_person : written_gear;
    if (!written) {
        ofstream f(path);
        f << svg_data;
        written = f.good();
    }
    return path;
}

static const char* kPersonSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 12c2.76 0 5-2.24 5-5s-2.24-5-5-5-5 2.24-5 5 2.24 5 5 5zm0 2c-3.33 0-10 1.67-10 5v2h20v-2c0-3.33-6.67-5-10-5z" fill="#646469"/>
</svg>)svg";

static const char* kGearSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M19.44 12.99c.04-.33.07-.66.07-1s-.03-.67-.07-1l2.44-1.92-2.32-4-2.82 1.17c-.5-.37-1.04-.69-1.63-.94l-.37-3h-4.64l-.38 3c-.59.25-1.12.57-1.62.94l-2.82-1.17-2.32 4 2.44 1.92c-.04.33-.07.66-.07 1s.03.67.07 1l-2.44 1.92 2.32 4 2.82-1.17c.5.37 1.04.69 1.63.94l.38 3h4.64l.38-3c.59-.25 1.12-.57 1.62-.94l2.82 1.17 2.32-4-2.44-1.92zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z" fill="#646469"/>
</svg>)svg";

static Image load_svg_icon(const char* name, const char* svg_data, int size)
{
    try {
        auto path = write_svg_tmp(name, svg_data);
        SvgImage svg("file:" + path, SizeF(size, size));
        return static_cast<Image>(svg);
    } catch (...) {
        return {};
    }
}

namespace {

// ── Start button: rounded rectangle with diagonal cyan→blue gradient ──────
// Matches Figma node 140:852 — the gradient runs top-left (light cyan) to
// bottom-right (deeper blue), with a soft drop shadow approximated by a
// slightly larger shadow Frame drawn behind. White bold "Start" label sits
// dead-centre. Pressed state darkens both gradient stops.
class StartButton : public Widget {
public:
    StartButton(const Rect& rect, const string& label, function<void()> on_click)
        : Widget(rect), m_label(label), m_on_click(std::move(on_click))
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);

        // Single handler covering press feedback + click. The "Start" text
        // is painted by this widget (see draw) rather than overlaid as a
        // separate Label — a Label on top would sit above the button in the
        // z-order and swallow the pointer_click, which is exactly why Start
        // used to do nothing while Demo Mode worked.
        on_event([this](Event& e) {
            switch (e.id()) {
                case EventId::raw_pointer_down: m_pressed = true;  damage(); break;
                case EventId::raw_pointer_up:   m_pressed = false; damage(); break;
                case EventId::pointer_click:    if (m_on_click) m_on_click(); break;
                default: break;
            }
        });
    }

    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = 14.0f;  // corner radius

        // Drop shadow — a faint dark rectangle offset 4 px down/right
        const float shadow_off = 4.0f;
        draw_rounded_path(painter, x + shadow_off, y + shadow_off, w, h, r);
        painter.set(Color(0, 0, 0, 40));
        painter.fill();

        // Gradient body
        Color top   = m_pressed ? Color(36, 130, 168) : Color(70, 178, 213);
        Color bot   = m_pressed ? Color(20,  90, 130) : Color(36, 117, 180);
        Pattern grad(Pattern::StepArray{{0.0f, top}, {1.0f, bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x + w), static_cast<int>(y + h)));
        draw_rounded_path(painter, x, y, w, h, r);
        painter.set(grad);
        painter.fill();

        // Centred white label, painted in-widget so it never intercepts the
        // click.
        painter.set(Color(255, 255, 255));
        painter.set(Font(40, Font::Weight::bold));
        const auto ts = painter.text_size(m_label);
        painter.draw(PointF(x + (w - ts.width()) / 2.0f,
                            y + (h - ts.height()) / 2.0f));
        painter.draw(m_label);
    }

private:
    bool m_pressed{false};
    string m_label;
    function<void()> m_on_click;

    // Build a rounded-rectangle path: top edge → top-right corner arc →
    // right edge → bottom-right corner arc → bottom edge → bottom-left corner
    // arc → left edge → top-left corner arc → close.
    static void draw_rounded_path(Painter& p, float x, float y,
                                  float w, float h, float r)
    {
        // Use draw(point)/line(point) (not move_to/line_to) so this builds
        // against both EGT 1.12 (host simulator) and EGT 1.10 (target),
        // where move_to/line_to aren't public Painter methods.
        const auto PI = static_cast<float>(M_PI);
        p.draw(PointF(x + r,         y));
        p.line(PointF(x + w - r,     y));
        p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
        p.line(PointF(x + w,         y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,   PI / 2));
        p.line(PointF(x + r,         y + h));
        p.draw(Arc(PointF(x + r, y + h - r),       r, PI / 2, PI));
        p.line(PointF(x,             y + r));
        p.draw(Arc(PointF(x + r, y + r),           r, PI,    3 * PI / 2));
    }
};

// ── Card with circular icon background + label (F1:1 against Figma) ────────
// Figma v5 HOME "Group 7" pattern (Demo Mode + Setting):
//   - DROP_SHADOW (offset 0,0, radius 10, rgba(0,0,0,0.10)) -> ShadowedCard
//   - card_rect 272x96, corner radius 7 (Figma 4 * SCALE)
//   - icon at card-local (13, 13), 72x72 (Figma 39 * SCALE) — pre-rendered
//     PNG already includes the gray circle background
//   - label at card-local (91, 33), 163x33 (Figma 88x18 * SCALE), Gothic A1
//     Bold 14 pt -> 26 pt, textAlignHorizontal=CENTER, textAlignVertical=TOP
//
// The PNG icon path is passed in so the same helper builds both cards.
shared_ptr<Frame> make_card_button(
    const Rect& card_rect,
    const string& icon_path,
    const string& text,
    function<void()> on_click)
{
    constexpr int PAD = ui::ShadowedCard::SHADOW_PAD;
    constexpr float card_radius = 7.0f;

    // Wrapper Frame sized to include the shadow extent.
    auto wrap = make_shared<Frame>(
        Rect(card_rect.x() - PAD, card_rect.y() - PAD,
             card_rect.width()  + 2 * PAD,
             card_rect.height() + 2 * PAD));
    wrap->fill_flags({});       // transparent

    // Shadowed white card + click handling.
    auto card = make_shared<ui::ShadowedCard>(
        Rect(PAD, PAD, card_rect.width(), card_rect.height()),
        card_radius,
        on_click);
    wrap->add(card);

    // Icon (PNG already includes the gray circle background).
    constexpr int icon_w = 72;
    constexpr int icon_h = 72;
    try {
        const float hscale = static_cast<float>(icon_w) / 78.0f;  // src PNG 78x78
        const float vscale = static_cast<float>(icon_h) / 78.0f;
        auto icon_img = Image(("file:" + icon_path).c_str(), hscale, vscale);
        auto icon_lbl = make_shared<ImageLabel>(icon_img);
        icon_lbl->autoresize(false);
        icon_lbl->border(0); icon_lbl->padding(0); icon_lbl->margin(0);
        icon_lbl->fill_flags({Theme::FillFlag::blend});
        icon_lbl->image_align(AlignFlag::center);
        icon_lbl->box(Rect(PAD + 13, PAD + 13, icon_w, icon_h));
        wrap->add(icon_lbl);
    } catch (const std::exception& e) {
        printf("[CARD] icon %s missing: %s\n", icon_path.c_str(), e.what());
        fflush(stdout);
    }

    // Label — span the full card height with center_vertical, like the
    // Continue button on WIFI_CONNECTED (matches the F1:1 recipe).
    auto lbl = make_shared<Label>(text,
        Rect(PAD + 91, PAD, 163, card_rect.height()));
    lbl->border(0); lbl->padding(0); lbl->margin(0);
    lbl->font(Font("Gothic A1", 26, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    wrap->add(lbl);

    return wrap;
}

} // namespace

// ── HOME / Begin Treatment screen ──────────────────────────────────────────
// Figma node 140:852 (Jason-M4.0 v5 — "Begin treatment"):
//   - Lice Clinics logo centred near the top
//   - Large "Start" button with a cyan→blue gradient in the middle
//   - Two cards at the bottom: "Demo Mode" (left) and "Setting" (right),
//     each with a gray circle + glyph + bold label
shared_ptr<Widget> create_home_screen(
    function<void()> on_begin_treatment,
    function<void()> on_demo_mode,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (Figma "Lice-temp logo 2" 21:133) ────────────────────────────
    // Figma 90x56 at (262, 191) -> 167x104 at (317, 20) scaled.
    auto logo = ui::create_logo(317, 20, 167, 104);
    container->add(logo);

    // ── Start button (Figma Group 46) ─────────────────────────────────────
    // Figma 140x73 at (238, 269) -> 259x135 at (272, 165) scaled.
    constexpr int btn_w = 259;
    constexpr int btn_h = 135;
    constexpr int btn_x = 272;
    constexpr int btn_y = 165;
    auto btn = make_shared<StartButton>(
        Rect(btn_x, btn_y, btn_w, btn_h), "Start", on_begin_treatment);
    container->add(btn);

    // ── Bottom cards (Demo Mode + Setting) — F1:1 mapped ──────────────────
    // Demo Mode card (Figma 102,377 147x52)  -> (20, 365)  272x96 scaled.
    // Setting card  (Figma 359,377 147x52)  -> (496, 365) 272x96 scaled.
    auto btn_demo = make_card_button(
        Rect(20, 365, 272, 96),
        "assets/figma/images/home-demo-icon.png",
        "Demo Mode",
        on_demo_mode);
    container->add(btn_demo);

    auto btn_setting = make_card_button(
        Rect(496, 365, 272, 96),
        "assets/figma/images/home-gear-icon.png",
        "Setting",
        on_settings);
    container->add(btn_setting);

    return container;
}
