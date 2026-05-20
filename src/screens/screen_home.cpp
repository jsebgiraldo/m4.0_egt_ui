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
        const auto PI = static_cast<float>(M_PI);
        p.move_to(PointF(x + r,         y));
        p.line_to(PointF(x + w - r,     y));
        p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
        p.line_to(PointF(x + w,         y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,   PI / 2));
        p.line_to(PointF(x + r,         y + h));
        p.draw(Arc(PointF(x + r, y + h - r),       r, PI / 2, PI));
        p.line_to(PointF(x,             y + r));
        p.draw(Arc(PointF(x + r, y + r),           r, PI,    3 * PI / 2));
    }
};

// ── Card with circular icon background + label ─────────────────────────────
// Figma cards (Group 263 / Group 264 patterns): rounded rectangle, white bg,
// soft gray border, soft drop shadow, a gray 56-px circle on the left holding
// the glyph, and a bold gray label to the right.
shared_ptr<Frame> make_card_button(
    const Rect& rect,
    const Image& icon,
    const string& text,
    function<void()> on_click)
{
    auto frame = make_shared<Frame>(rect);
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, dt::kWhite);
    frame->border(1);
    frame->color(Palette::ColorId::border, dt::kGrayLight);
    frame->border_radius(dt::RADIUS_MD);

    // Circular gray background for the icon — Figma "Ellipse 1" 39×39 pt → 56 px
    const int circle_d = 56;
    const int circle_x = 18;
    const int circle_y = (rect.height() - circle_d) / 2;
    auto circle_bg = make_shared<Frame>(Rect(circle_x, circle_y, circle_d, circle_d));
    circle_bg->fill_flags({Theme::FillFlag::blend});
    circle_bg->color(Palette::ColorId::bg, palette::kGray200);
    circle_bg->border(0);
    circle_bg->border_radius(circle_d / 2);
    frame->add(circle_bg);

    // Glyph centred inside the circle. Clear fill_flags + set the bg to the
    // circle colour so the SVG's transparent areas blend cleanly into the
    // disc — without this the ImageLabel draws its default white bg behind
    // the glyph and you see a white square halo inside the gray circle.
    if (!icon.empty()) {
        const int icon_sz = 32;
        auto icon_lbl = make_shared<ImageLabel>(icon);
        icon_lbl->fill_flags({});
        icon_lbl->color(Palette::ColorId::bg, palette::kGray200);
        icon_lbl->image_align(AlignFlag::center);
        icon_lbl->move(Point(circle_x + (circle_d - icon_sz) / 2,
                             circle_y + (circle_d - icon_sz) / 2));
        icon_lbl->resize(Size(icon_sz, icon_sz));
        frame->add(icon_lbl);
    }

    // Label
    const int label_x = circle_x + circle_d + 16;
    const int label_w = rect.width() - label_x - 10;
    auto lbl = make_shared<Label>(text, Rect(label_x, 0, label_w, rect.height()));
    lbl->font(Font(17, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    frame->add(lbl);

    if (on_click)
        frame->on_event([on_click](Event&) { on_click(); }, {EventId::pointer_click});

    return frame;
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

    // ── Logo (centred near the top) ───────────────────────────────────────
    // Figma Group 197 ≈ 90×56 pt → ~166×103 px scaled.
    const int logo_w = 166;
    const int logo_h = 103;
    auto logo = ui::create_logo(
        (dt::SCREEN_W - logo_w) / 2,
        38,
        logo_w,
        logo_h);
    container->add(logo);

    // ── Start button (centred, gradient cyan→blue) ─────────────────────────
    // Figma button: ~131×60 pt → ~243×111 px scaled; sits about y=85 → 158.
    const int btn_w = 250;
    const int btn_h = 110;
    const int btn_x = (dt::SCREEN_W - btn_w) / 2;
    const int btn_y = 162;
    auto btn = make_shared<StartButton>(
        Rect(btn_x, btn_y, btn_w, btn_h), "Start", on_begin_treatment);
    container->add(btn);

    // ── Bottom cards (Demo Mode + Setting) ────────────────────────────────
    // Figma cards: ~165×40 pt → ~306×74 px each; bottom strip y≈193 → 357.
    const int card_w = 320;
    const int card_h = 88;
    const int card_y = 360;
    const int gap    = dt::SCREEN_W - 2 * card_w - 30 * 2;  // even outside margins

    auto icon_person = load_svg_icon("person", kPersonSvg, 32);
    auto icon_gear   = load_svg_icon("gear",   kGearSvg,   32);

    auto btn_demo = make_card_button(
        Rect(30, card_y, card_w, card_h),
        icon_person,
        "Demo Mode",
        on_demo_mode);
    container->add(btn_demo);

    auto btn_setting = make_card_button(
        Rect(30 + card_w + gap, card_y, card_w, card_h),
        icon_gear,
        "Setting",
        on_settings);
    container->add(btn_setting);

    return container;
}
