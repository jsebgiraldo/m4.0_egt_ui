#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_patient_info.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <cstdlib>
#include <fstream>

using namespace egt;
using namespace std;

// ── SVG icon helpers ─────────────────────────────────────────────────────────
static string write_svg_tmp(const char* name, const char* svg_data)
{
    string path = string("/tmp/egt-icon-") + name + ".svg";
    ofstream f(path);
    f << svg_data;
    return path;
}

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

// Flow-dependent accent colour (ToDo master task 4): blue/cyan for Demo
// Mode, green for the real (START) treatment flow. Applied to the primary
// action buttons (Continue / GO) so the colour signals which flow you're in.
static egt::Color flow_accent(bool demo) {
    return demo ? dt::kAccentCyan : dt::kGreen;
}

// Vertical wheel-picker backdrop (Figma 2009:1208 Rectangles 73/74): two
// stacked vertical gradients that darken the top and bottom edges and fade to
// near-white in the middle, giving the age wheel its cylinder look. Drawn by
// hand because EGT's flat Palette fill can't express a multi-stop gradient.
class WheelBackdrop : public Widget {
public:
    explicit WheelBackdrop(const Rect& r) : Widget(r) {
        fill_flags({});   // fully custom-drawn, no theme fill
    }
    void draw(Painter& painter, const Rect&) override {
        const auto b = content_area();
        const int x = b.x(), y = b.y(), w = b.width(), h = b.height();
        // Rectangle 74: light-gray fade - opaque at top/bottom, clear middle.
        painter.draw(Pattern(Pattern::StepArray{
            {0.00f, Color(217, 217, 217, 210)},
            {0.33f, Color(217, 217, 217, 0)},
            {0.58f, Color(217, 217, 217, 0)},
            {1.00f, Color(217, 217, 217, 210)}},
            Point(x, y), Point(x, y + h)),
            RectF(x, y, w, h));
        // Rectangle 73: subtle cylinder shading over the top/bottom edges.
        // Figma uses 0.9 alpha here, but stacked over the light-gray layer
        // above that reads near-black on our display; a low alpha matches the
        // soft gray the Figma frame actually renders.
        painter.draw(Pattern(Pattern::StepArray{
            {0.00f, Color(100, 101, 105, 40)},
            {0.38f, Color(255, 255, 255, 20)},
            {0.55f, Color(255, 255, 255, 20)},
            {1.00f, Color(100, 101, 105, 40)}},
            Point(x, y), Point(x, y + h)),
            RectF(x, y, w, h));
    }
};

// Male / Female silhouettes — disc + glyph composition.
// Source SVGs live in assets/icons/{male,female}.svg; these inlined copies
// keep the binary self-contained.
// Single variant for both selected and unselected cards (gray disc + gray figure):
// the contrast comes from the surrounding card bg, not from the icon.
// Glyph scaled 0.75 inside the 24x24 viewBox (translate 3,3 keeps it centered).

static const char* kMaleSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#E8E8E8"/>
  <g transform="translate(3,3) scale(0.75)">
    <circle cx="12" cy="5.5" r="2.5" fill="#646469"/>
    <rect x="7.5" y="9" width="9" height="8" rx="1.2" fill="#646469"/>
    <rect x="8.5" y="16.5" width="2.6" height="5.5" rx="0.6" fill="#646469"/>
    <rect x="12.9" y="16.5" width="2.6" height="5.5" rx="0.6" fill="#646469"/>
  </g>
</svg>)svg";

static const char* kFemaleSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#E8E8E8"/>
  <g transform="translate(3,3) scale(0.75)">
    <circle cx="12" cy="5.5" r="2.5" fill="#646469"/>
    <path d="M7 9 Q7 8.5 7.5 8.5 H16.5 Q17 8.5 17 9 L19 18 H5 L7 9 Z" fill="#646469"/>
    <rect x="9" y="18" width="2.4" height="4.2" rx="0.6" fill="#646469"/>
    <rect x="12.6" y="18" width="2.4" height="4.2" rx="0.6" fill="#646469"/>
  </g>
</svg>)svg";

// Bottom-button glyphs (Figma 2009:1262 Back/Skip/Continue). Each is a disc
// with the 217->255 vertical gradient (Figma Ellipse 4 "fill_1SIERY") plus the
// exact glyph path downloaded from Figma. Glyph fill is #646569 on all three
// (yes, including Continue — Figma "Subtract" is gray, not white). The disc
// gradient + glyph match the design 1:1 instead of the old hand-drawn discs.

// Back: Figma "Subtract" chevron, native 9x13, centred in the 24x24 disc.
static const char* kArrowBackSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <defs><linearGradient id="d" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#D9D9D9"/><stop offset="100%" stop-color="#FFFFFF"/>
  </linearGradient></defs>
  <circle cx="12" cy="12" r="12" fill="url(#d)"/>
  <g transform="translate(7.4,5.5)">
    <path fill-rule="evenodd" clip-rule="evenodd" d="M5.45559 0.541444C6.53804 -0.468095 8.19841 0.0266318 8.68352 1.26245L4.63786 5.03651C3.78963 5.8276 3.78963 7.17243 4.63786 7.96353L8.68352 11.7366C8.1988 12.973 6.53827 13.4683 5.45559 12.4586L0.636172 7.96353C-0.212057 7.17243 -0.212057 5.8276 0.636172 5.03651L5.45559 0.541444Z" fill="#646569"/>
  </g>
</svg>)svg";

// Skip: Figma "Vector 3980" (curved-arrow loop), native 15x14 stroked path.
static const char* kSkipNextSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <defs><linearGradient id="d" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#D9D9D9"/><stop offset="100%" stop-color="#FFFFFF"/>
  </linearGradient></defs>
  <circle cx="12" cy="12" r="12" fill="url(#d)"/>
  <g transform="translate(4.5,5)">
    <path d="M1.22135 12.7448C1.22135 5.7028 0.697507 1.20632 5.82135 1.20632C10.9452 1.20632 9.6029 8.24831 9.6029 12.7448M5.82135 8.75131L9.6029 12.7448L13.7937 7.74531" stroke="#646569" stroke-width="2.3" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
  </g>
</svg>)svg";

// Continue: Figma "Subtract" chevron (">"), native 9x13, centred in the disc.
static const char* kArrowFwdSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <defs><linearGradient id="d" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#D9D9D9"/><stop offset="100%" stop-color="#FFFFFF"/>
  </linearGradient></defs>
  <circle cx="12" cy="12" r="12" fill="url(#d)"/>
  <g transform="translate(7.8,5.5)">
    <path fill-rule="evenodd" clip-rule="evenodd" d="M3.22796 12.4586C2.14551 13.4681 0.48513 12.9734 2.18335e-05 11.7375L4.0457 7.96347C4.89393 7.17237 4.89393 5.82754 4.0457 5.03644L2.09178e-05 1.26334C0.484738 0.0269478 2.14528 -0.468401 3.22796 0.541353L8.0474 5.03644C8.89564 5.82754 8.89564 7.17237 8.04741 7.96347L3.22796 12.4586Z" fill="#646569"/>
  </g>
</svg>)svg";

// Disc + refresh/reset — Reset glyph (Material Symbols refresh)
static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#E8E8E8"/>
  <g transform="translate(3,3) scale(0.75)">
    <path d="M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646469"/>
  </g>
</svg>)svg";

// ── Soft drop shadow ────────────────────────────────────────────────────────
// Figma boxShadow 0px 0px 10px rgba(0,0,0,0.1) on the gender cards and the
// Back/Skip/Continue buttons. EGT has no blur, so (like ui::ShadowedCard) we
// fake it with 16 concentric translucent-black rounded rects. Unlike
// ShadowedCard this draws NO card fill, so it sits behind any colour card
// (white, green, or the blue Continue). Add it to the parent BEFORE the card.
static void pi_rounded_path(Painter& p, float x, float y, float w, float h, float r)
{
    const float PI = static_cast<float>(M_PI);
    p.draw(PointF(x + r,         y));
    p.line(PointF(x + w - r,     y));
    p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
    p.line(PointF(x + w,         y + h - r));
    p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,    PI / 2));
    p.line(PointF(x + r,         y + h));
    p.draw(Arc(PointF(x + r,     y + h - r),   r, PI / 2,  PI));
    p.line(PointF(x,             y + r));
    p.draw(Arc(PointF(x + r,     y + r),       r, PI,    3 * PI / 2));
}

class SoftShadow : public Widget {
public:
    static constexpr int PAD = 12;
    SoftShadow(const Rect& card, float radius)
        : Widget(Rect(card.x() - PAD, card.y() - PAD,
                      card.width() + 2 * PAD, card.height() + 2 * PAD)),
          m_radius(radius), m_card(card) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        readonly(true);            // never interactive — taps fall to the card
    }
    void draw(Painter& p, const Rect&) override {
        const float x = m_card.x(), y = m_card.y();
        const float w = m_card.width(), h = m_card.height(), r = m_radius;
        // Softer than ShadowedCard: alpha 1 per layer (vs 2) over a slightly
        // wider 10px spread, so the edge reads as a faint halo, not a dark rim.
        constexpr int steps = 18; constexpr float extent = 10.0f;
        for (int i = steps; i >= 1; --i) {
            const float grow = i * (extent / steps);
            pi_rounded_path(p, x - grow, y - grow,
                            w + 2.0f * grow, h + 2.0f * grow, r + grow * 0.5f);
            p.set(Color(0, 0, 0, 1));
            p.fill();
        }
    }
private:
    float m_radius;
    Rect  m_card;
};

// Figma Group 231: Patient Information — 3 sub-screens (Gender, Age, ZIP)
// Canvas: 432×261, screen: 800×480 → scale ≈ 1.852

// ── Forward declarations ────────────────────────────────────────────────────
static shared_ptr<Widget> create_gender_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo);

static shared_ptr<Widget> create_age_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo);

static shared_ptr<Widget> create_zip_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo);

static shared_ptr<Widget> create_summary_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back_to_zip,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo);

// ── Green "Continue" button (Figma: bg=#5BC500, white text) ─────────────────
static shared_ptr<Button> create_green_button(
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto btn = make_shared<Button>(text, rect);
    btn->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
    btn->color(Palette::ColorId::button_bg, dt::kGreen);
    btn->color(Palette::ColorId::button_text, dt::kWhite);
    btn->border(0);
    btn->border_radius(dt::RADIUS_SM);
    if (on_click) {
        btn->on_click([on_click](Event&) { on_click(); });
    }
    return btn;
}

// Inactive ZIP keypad key: soft gray vertical gradient (Figma "number bt"
// fill, 217 -> 255 top to bottom), no border. Matches the "key number"
// component default state used on node 2009:1060.
static shared_ptr<Button> create_gradient_key(
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto btn = make_shared<Button>(text, rect);
    btn->font(Font(24, Font::Weight::bold));
    Pattern grad(Pattern::StepArray{{0.0f, Color(217, 217, 217)},
                                    {1.0f, Color(255, 255, 255)}},
                 Point(rect.x(), rect.y()),
                 Point(rect.x(), rect.y() + rect.height()));
    btn->color(Palette::ColorId::button_bg, grad);
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->border(0);
    btn->border_radius(dt::RADIUS_SM);
    if (on_click) {
        btn->on_click([on_click](Event&) { on_click(); });
    }
    return btn;
}

// ── Icon button helpers ───────────────────────────────────────────────────────
// Outlined button: off-white bg, 1 px gray border for separation from page bg
static shared_ptr<ImageButton> make_icon_outlined_btn(
    const char* icon_name, const char* icon_svg,
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto ico = load_svg_icon(icon_name, icon_svg, 44);
    auto btn = make_shared<ImageButton>(ico, text, rect, AlignFlag::center);
    btn->image_align(AlignFlag::left | AlignFlag::center_vertical);
    // Pin the min size to the rect so the 44px icon doesn't auto-grow the
    // button past the Figma 33px(->61) height.
    btn->min_size_hint(Size(rect.width(), rect.height()));
    btn->font(Font(22, Font::Weight::bold));   // Figma 14pt bold, same as Male/Female labels
    btn->color(Palette::ColorId::button_bg, Color(0xFA, 0xFA, 0xFA));
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->color(Palette::ColorId::border, dt::kGrayLight);
    btn->border(1);
    btn->border_radius(dt::RADIUS_XS);
    if (on_click) btn->on_click([on_click](Event&) { on_click(); });
    return btn;
}

// Filled button with a leading disc+glyph icon (white icon/text on `bg` color)
static shared_ptr<ImageButton> make_icon_filled_btn(
    const char* icon_name, const char* icon_svg,
    const string& text, const Rect& rect, function<void()> on_click,
    const Color& bg = dt::kGreen)
{
    auto ico = load_svg_icon(icon_name, icon_svg, 44);
    auto btn = make_shared<ImageButton>(ico, text, rect, AlignFlag::center);
    btn->image_align(AlignFlag::left | AlignFlag::center_vertical);
    btn->min_size_hint(Size(rect.width(), rect.height()));
    btn->font(Font(22, Font::Weight::bold));   // Figma 14pt bold, same as Male/Female labels
    btn->color(Palette::ColorId::button_bg, bg);
    btn->color(Palette::ColorId::button_text, dt::kWhite);
    btn->border(0);
    btn->border_radius(dt::RADIUS_XS);
    if (on_click) btn->on_click([on_click](Event&) { on_click(); });
    return btn;
}

// Continue button with a "required field filled" gate (ToDo master task 5).
// When `enabled` is false it renders gray and ignores taps; when true it
// uses the flow accent (green real / blue demo) and runs `on_click`.
static shared_ptr<ImageButton> make_continue_btn(
    bool enabled, bool demo, const Rect& rect, function<void()> on_click)
{
    auto btn = make_icon_filled_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "  Continue", rect,
        enabled ? on_click : function<void()>(nullptr),
        enabled ? flow_accent(demo) : dt::kGrayLight);
    if (!enabled) {
        // Dim the label/icon text so the disabled state reads clearly.
        btn->color(Palette::ColorId::button_text, palette::kGray500);
    }
    return btn;
}

// ── Common header + tab bar (Figma layout) ──────────────────────────────────
// step: 0=Gender, 1=Age, 2=ZIP
// nav_to_step: optional callback to navigate when a tab is clicked
static shared_ptr<Frame> make_patient_step(
    int step, bool demo_mode, function<void()> on_leave_demo,
    function<void(int)> nav_to_step = nullptr,
    bool as_pills = false)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo @(4,7, 167×104)
    auto logo = ui::create_logo(4, 7, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Title "Please Enter Client Information" @(244,17) — Figma node 115:960
    auto title = make_shared<Label>("Please Enter Client Information",
        Rect(244, 17, 350, 46),
        AlignFlag::center_vertical | AlignFlag::left);
    title->font(Font(22, Font::Weight::normal));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // Small profile icon to the right of the title (Figma node 115:961,
    // 15x15 figma px at frame-local (323, 14) -> device (598, 26, 28x28).
    try {
        auto img = Image("file:assets/figma/images/patient-title-profile.png");
        auto profile = make_shared<ImageLabel>(img);
        profile->autoresize(false);
        profile->border(0); profile->padding(0); profile->margin(0);
        profile->fill_flags({});
        profile->image_align(AlignFlag::center);
        profile->box(Rect(598, 26, 28, 28));
        container->add(profile);
    } catch (...) { /* fall back to no icon */ }

    // Tab labels (progressively shown: step 0 → Gender only, step 1 → +Age, step 2 → +ZIP)
    const char* tab_names[] = {"Gender", "Age", "ZIP Code"};
    const int tab_x[] = {222, 404, 531};
    const int tab_w[] = {70, 40, 90};
    const int green_bar_x[] = {189, 352, 509};

    if (as_pills) {
        // Summary step (Figma 2009:962): all three steps shown as completed
        // white pill tabs, each with a small radius + 1px outline.
        const int pill_x[] = {202, 367, 531};
        const int pill_w = 107, pill_y = 72, pill_h = 36;
        for (int i = 0; i < 3; i++) {
            auto pill = make_shared<Frame>(Rect(pill_x[i], pill_y, pill_w, pill_h));
            pill->fill_flags({Theme::FillFlag::blend});
            pill->color(Palette::ColorId::bg, dt::kBgWhite);
            pill->color(Palette::ColorId::border, dt::kGrayLight);
            pill->border(1);
            pill->border_radius(dt::RADIUS_XS);
            auto lbl = make_shared<Label>(tab_names[i],
                Rect(0, 0, pill_w, pill_h), AlignFlag::center);
            lbl->font(Font(18, Font::Weight::normal));
            lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
            pill->add(lbl);
            container->add(pill);
        }
    } else {
        for (int i = 0; i <= step; i++) {
            // y=75 (was 70): drops the label ~5px so its gap to the green bar
            // matches Figma (text vertical centre ~y90, node 2009:1303 y=38).
            auto tab = make_shared<Label>(tab_names[i],
                Rect(tab_x[i], 75, tab_w[i], 30),
                AlignFlag::center_vertical | AlignFlag::left);
            tab->font(Font(18, (i == step) ? Font::Weight::bold : Font::Weight::normal));
            tab->color(Palette::ColorId::label_text, dt::kTextPrimary);
            container->add(tab);

            // Make previous tabs clickable for navigation
            if (i < step && nav_to_step) {
                int target = i;
                tab->on_event([nav_to_step, target](Event& event) {
                    if (event.id() == EventId::pointer_click) {
                        nav_to_step(target);
                    }
                });
            }
        }
    }

    // Divider line @(0,111, 800×2)
    auto divider = make_shared<Frame>(Rect(0, 111, 800, 2));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    if (as_pills) {
        // Full-width green bar: every step complete (Figma Rectangle 32).
        auto green = make_shared<Frame>(Rect(2, 109, 796, 6));
        green->fill_flags({Theme::FillFlag::blend});
        green->color(Palette::ColorId::bg, dt::kGreen);
        green->border(0);
        container->add(green);
    } else {
        // Green indicator bar under active tab @(green_bar_x, 109, 133×6)
        auto indicator = make_shared<Frame>(Rect(green_bar_x[step], 109, 133, 6));
        indicator->fill_flags({Theme::FillFlag::blend});
        indicator->color(Palette::ColorId::bg, dt::kGreen);
        indicator->border(0);
        container->add(indicator);
    }

    // Demo badge (vertical: DEMO MODE label + Exit below)
    if (demo_mode && on_leave_demo) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 104, 12, on_leave_demo,
            ui::DemoBadgeStyle::Compact);
        container->add(badge.frame);
    }

    return container;
}

// ── Entry point ─────────────────────────────────────────────────────────────
shared_ptr<Widget> create_patient_info_screen(
    bool demo_mode,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    auto info = make_shared<PatientInfo>();
    return create_gender_step(demo_mode, info, on_complete, on_back,
                              on_show_screen, on_leave_demo);
}

// ── Step 1: Gender (two cards) ──────────────────────────────────────────────
// Figma node 168:808 — two side-by-side cards, selected = green fill + white
static shared_ptr<Widget> create_gender_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    auto container = make_patient_step(0, demo_mode, on_leave_demo);

    // Tri-state: empty = neither card selected (after Skip / first visit),
    // "Male" or "Female" = that card highlighted. We never auto-commit a
    // default — that was the bug that made Skip leak "Male" into the
    // summary even though the user explicitly opted out.
    const bool has_gender = !info->gender.empty();
    const bool is_female  = (info->gender == "Female");

    auto select_and_rebuild = [=](bool female) {
        info->gender = female ? "Female" : "Male";
        if (on_show_screen)
            on_show_screen(create_gender_step(demo_mode, info, on_complete,
                on_back, on_show_screen, on_leave_demo));
    };

    // Figma Group 254/255: 92.84x73 -> 172x134 (SCALE_X=1.852, SCALE_Y=1.836).
    // The cards sit ~8px right of screen centre in Figma, so place absolutely.
    const int card_w = 172, card_h = 134, gap = 36;
    const int start_x = 218;             // Male x=118*1.852; Female lands at 426
    const int card_y = 189;              // y=103*1.836
    const int icon_sz = 46, icon_y = 26; // Group 252 24.63*1.852, y=14*1.836

    auto build_card = [&](bool female_card) {
        const bool selected = has_gender && (female_card == is_female);
        const int x = start_x + (female_card ? (card_w + gap) : 0);

        // Soft drop shadow behind the card (Figma Group 7 boxShadow).
        container->add(make_shared<SoftShadow>(
            Rect(x, card_y, card_w, card_h), dt::RADIUS_XS));

        auto card = make_shared<Frame>(Rect(x, card_y, card_w, card_h));
        card->fill_flags({Theme::FillFlag::blend});
        card->color(Palette::ColorId::bg,
            selected ? dt::kGreen : Color(0xFA, 0xFA, 0xFA));
        if (selected) {
            card->border(0);
        } else {
            card->color(Palette::ColorId::border, dt::kGrayLight);
            card->border(1);
        }
        card->border_radius(dt::RADIUS_XS);
        container->add(card);

        // Icon from Figma PNG (Group 252 / Group 253). The custom SVG
        // silhouettes did not match the design - swapped for the actual
        // figma exports so the Male / Female glyphs read identically to
        // the figma render.
        const std::string icon_path = female_card
            ? "assets/figma/images/patient-female-icon.png"
            : "assets/figma/images/patient-male-icon.png";
        try {
            auto img = Image(("file:" + icon_path).c_str());
            auto icon = make_shared<ImageLabel>(img);
            icon->autoresize(false);
            icon->border(0); icon->padding(0); icon->margin(0);
            icon->fill_flags({});
            icon->image_align(AlignFlag::center);
            const int icon_x = (card_w - icon_sz) / 2;
            icon->box(Rect(icon_x, icon_y, icon_sz, icon_sz));
            card->add(icon);
        } catch (...) { /* fall back: no icon */ }
        (void)kMaleSvg; (void)kFemaleSvg;  // SVG constants kept but unused

        auto lbl = make_shared<Label>(female_card ? "Female" : "Male",
            Rect(0, icon_y + icon_sz + 8, card_w, 32), AlignFlag::center);
        lbl->font(Font(22, Font::Weight::bold));
        lbl->color(Palette::ColorId::label_text,
            selected ? dt::kWhite : dt::kTextPrimary);
        card->add(lbl);

        card->on_event([=](Event& event) {
            if (event.id() == EventId::pointer_click) select_and_rebuild(female_card);
        });
    };

    build_card(false); // Male
    build_card(true);  // Female

    // Bottom buttons: Back @(42,400) Skip @(292,400) Continue @(541,400).
    // Each gets a soft drop shadow behind it (Figma Group 7 boxShadow).
    const Rect back_r(26, 397, 156, 61);   // Figma x=14*1.852, y=216*1.836
    const Rect skip_r(293, 397, 156, 61);  // x=158*1.852
    const Rect cont_r(556, 397, 217, 61);  // x=300*1.852

    container->add(make_shared<SoftShadow>(back_r, dt::RADIUS_XS));
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "  Back", back_r, on_back);
    container->add(btn_back);

    container->add(make_shared<SoftShadow>(skip_r, dt::RADIUS_XS));
    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip", skip_r,
        [=]() {
            info->gender.clear();   // skip => no value collected
            if (on_show_screen)
                on_show_screen(create_age_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    // Continue gated on a gender being selected (task 5).
    container->add(make_shared<SoftShadow>(cont_r, dt::RADIUS_XS));
    auto btn_continue = make_continue_btn(
        !info->gender.empty(), demo_mode, cont_r,
        [=]() {
            if (on_show_screen)
                on_show_screen(create_age_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_continue);

    return container;
}

// ── Step 2: Age range picker ────────────────────────────────────────────────
// Figma: scrollable range columns "1|5" "6|12" etc. with left/right arrows
static shared_ptr<Widget> create_age_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    // Navigation callback for tab clicks
    auto nav_to_step = [=](int target) {
        if (target == 0 && on_show_screen)
            on_show_screen(create_gender_step(demo_mode, info, on_complete,
                on_back, on_show_screen, on_leave_demo));
    };
    auto container = make_patient_step(1, demo_mode, on_leave_demo, nav_to_step);

    // Age ranges (Figma Group 204 — vertical wheel picker labels)
    struct AgeRange { const char* label; int low; };
    const AgeRange ranges[] = {
        {"under 5", 1}, {"6 - 12", 6}, {"13 - 18", 13},
        {"19 - 29", 19}, {"30 - 49", 30}, {"50+", 50},
    };
    const int num_ranges = 6;

    // Default visible selection: 13-18 (index 2) when no age is set yet.
    // IMPORTANT: we no longer commit info->age here — the value is only
    // written when the user explicitly clicks Continue (or drags the
    // picker). Otherwise Skip would leak the default into the summary.
    int sel = 2;
    for (int i = 0; i < num_ranges; i++) {
        if (info->age == ranges[i].low) { sel = i; break; }
    }

    // Picker placed BELOW the tab divider+indicator (y=115) so the green
    // active-tab bar stays visible.
    const int slot_h    = 38;
    const int n_slots   = 5;             // odd \u2192 middle slot is the selected one
    const int chevron_h = 18;
    const int padding   = 6;
    const int box_w     = 240;
    const int box_h     = n_slots * slot_h + 2 * (chevron_h + padding);
    const int box_x     = (dt::SCREEN_W - box_w) / 2;
    const int box_y     = 130;
    auto picker_box = make_shared<Frame>(Rect(box_x, box_y, box_w, box_h));
    picker_box->fill_flags({Theme::FillFlag::blend});
    picker_box->color(Palette::ColorId::bg, dt::kBgWhite);
    picker_box->border(0);
    container->add(picker_box);

    // Cylinder gradient backdrop (Figma Rectangles 73/74), drawn behind the
    // wheel text. Added first so it sits under the slot labels and arrows.
    const int bd_w = 192;   // Figma 103px gradient rect * SCALE
    picker_box->add(make_shared<WheelBackdrop>(
        Rect((box_w - bd_w) / 2, 0, bd_w, box_h)));

    // Roller ruler ticks (Figma "rull" 2009:1210): a short gray dash on the
    // left of each row so the wheel reads as a physical roller/dial. These are
    // fixed (they don't scroll with the values).
    {
        const int slots_top0 = chevron_h + padding;
        const int dash_x = 52, dash_w = 10, dash_h = 2;
        for (int k = 0; k < n_slots; k++) {
            const int cy = slots_top0 + k * slot_h + slot_h / 2;
            auto dash = make_shared<Frame>(
                Rect(dash_x, cy - dash_h / 2, dash_w, dash_h));
            dash->fill_flags({Theme::FillFlag::blend});
            dash->color(Palette::ColorId::bg, palette::kGray500);
            dash->border(0);
            picker_box->add(dash);
        }
    }

    auto up_arrow = make_shared<Label>("\u25B2",
        Rect(0, 4, box_w, chevron_h), AlignFlag::center);
    up_arrow->font(Font(14));
    up_arrow->color(Palette::ColorId::label_text, palette::kGray400);
    picker_box->add(up_arrow);

    auto down_arrow = make_shared<Label>("\u25BC",
        Rect(0, box_h - chevron_h - 4, box_w, chevron_h), AlignFlag::center);
    down_arrow->font(Font(14));
    down_arrow->color(Palette::ColorId::label_text, palette::kGray400);
    picker_box->add(down_arrow);

    // FIXED slot frames render the wheel. They never move; a transparent
    // vertical Slider on top drives `sel` with live_update — same pattern
    // we used for brightness, which is the only way EGT emits continuous
    // value-change events during a drag.
    const int slots_top       = chevron_h + padding;
    const int center_slot_idx = n_slots / 2;

    auto slot_labels = make_shared<vector<shared_ptr<Label>>>();

    for (int k = 0; k < n_slots; k++) {
        const int slot_y = slots_top + k * slot_h;
        auto rf = make_shared<Frame>(Rect(0, slot_y, box_w, slot_h));
        rf->fill_flags({Theme::FillFlag::blend});
        rf->color(Palette::ColorId::bg, dt::kTransparent);
        rf->border(0);

        auto lbl = make_shared<Label>("",
            Rect(0, 0, box_w, slot_h), AlignFlag::center);
        rf->add(lbl);
        picker_box->add(rf);

        slot_labels->push_back(lbl);
    }

    // Slot k displays ranges[sel + (k - center_slot_idx)] when in range.
    auto redraw = [=](int state) {
        for (int k = 0; k < n_slots; k++) {
            const int idx = state + (k - center_slot_idx);
            if (idx < 0 || idx >= num_ranges) {
                (*slot_labels)[k]->text("");
                continue;
            }
            const bool is_sel = (k == center_slot_idx);
            const int  dist   = std::abs(k - center_slot_idx);
            (*slot_labels)[k]->text(ranges[idx].label);
            (*slot_labels)[k]->font(Font(
                is_sel ? 28 : 22,
                is_sel ? Font::Weight::bold : Font::Weight::normal));
            const Color c =
                is_sel        ? dt::kTextPrimary
              : (dist == 1)   ? palette::kGray500
                              : palette::kGray400;
            (*slot_labels)[k]->color(Palette::ColorId::label_text, c);
        }
        picker_box->damage();
    };

    redraw(sel);

    // Vertical Slider overlay — invisible, drives the wheel in real time.
    // Slider value = sel directly. Default vertical slider: drag DOWN
    // decreases value (handle moves toward bottom = min) → sel decreases,
    // matching the iOS-picker convention.
    auto picker_slider = make_shared<Slider>(
        Rect(box_x, box_y + slots_top, box_w, n_slots * slot_h),
        0, num_ranges - 1, sel,
        Orientation::vertical);
    picker_slider->live_update(true);
    picker_slider->fill_flags({});           // no background fill
    picker_slider->border(0);
    // Force every slider sub-element transparent in every Palette group so
    // the handle / track stays hidden in normal, pressed (active), disabled,
    // checked states (handle dragged = active group).
    for (auto group : {Palette::GroupId::normal,   Palette::GroupId::active,
                       Palette::GroupId::disabled, Palette::GroupId::checked}) {
        picker_slider->color(Palette::ColorId::button_bg, dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::button_fg, dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::border,    dt::kTransparent, group);
        picker_slider->color(Palette::ColorId::bg,        dt::kTransparent, group);
    }
    container->add(picker_slider);  // added to container → on top of picker_box

    auto live = make_shared<int>(sel);
    picker_slider->on_value_changed([=]() {
        const int v = picker_slider->value();
        if (v != *live) {
            *live = v;
            redraw(v);
            // intentionally NOT committing info->age here — Continue does
            // that. This keeps Skip honest even if the user dragged before
            // changing their mind.
        }
    });

    // "Years Old" caption to the right of the wheel (Figma 2009:1177, Bold
    // 10pt #646569, vertically aligned with the selected row).
    auto years_lbl = make_shared<Label>("Years Old",
        Rect(box_x + box_w + 5, box_y + box_h / 2 - 16, 120, 32),
        AlignFlag::left | AlignFlag::center_vertical);
    years_lbl->font(Font(22, Font::Weight::bold));
    years_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(years_lbl);

    // Bottom buttons (same Figma layout/shadows as the Gender step).
    const Rect back_r(26, 397, 156, 61);
    const Rect skip_r(293, 397, 156, 61);
    const Rect cont_r(556, 397, 217, 61);

    container->add(make_shared<SoftShadow>(back_r, dt::RADIUS_XS));
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "  Back", back_r,
        [=]() {
            if (on_show_screen)
                on_show_screen(create_gender_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_back);

    container->add(make_shared<SoftShadow>(skip_r, dt::RADIUS_XS));
    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip", skip_r,
        [=]() {
            info->age = 0;  // skip => no value collected
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    container->add(make_shared<SoftShadow>(cont_r, dt::RADIUS_XS));
    auto btn_continue = make_icon_filled_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "  Continue", cont_r,
        [=]() {
            info->age = ranges[*live].low;  // commit the visible selection
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        },
        flow_accent(demo_mode));
    container->add(btn_continue);

    return container;
}

// ── Step 3: ZIP Code (5×2 keypad) ───────────────────────────────────────────
// Figma: 5 columns × 2 rows of 74×74 keys, digits 0-9
static shared_ptr<Widget> create_zip_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    // Navigation callback for tab clicks
    auto nav_to_step = [=](int target) {
        if (!on_show_screen) return;
        if (target == 0)
            on_show_screen(create_gender_step(demo_mode, info, on_complete,
                on_back, on_show_screen, on_leave_demo));
        else if (target == 1)
            on_show_screen(create_age_step(demo_mode, info, on_complete,
                on_back, on_show_screen, on_leave_demo));
    };
    auto container = make_patient_step(2, demo_mode, on_leave_demo, nav_to_step);

    // ZIP display above keypad
    string display_str;
    for (size_t i = 0; i < 5; i++) {
        if (i > 0) display_str += "  ";
        if (i < info->zip_code.length())
            display_str += info->zip_code[i];
        else
            display_str += "_";
    }
    auto zip_display = make_shared<Label>(display_str,
        Rect(0, 125, 800, 46), AlignFlag::center);
    zip_display->font(Font(33, Font::Weight::bold));   // Figma 18pt * 1.852
    zip_display->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(zip_display);

    // Keypad: 5 cols × 2 rows, keys 74×74
    // Row 1: 0,1,2,3,4 @y=176  Row 2: 5,6,7,8,9 @y=265
    const int key_sz = 74;
    const int key_xs[] = {178, 274, 370, 467, 561};
    const int row_ys[] = {176, 265};

    for (int digit = 0; digit <= 9; digit++) {
        int col = digit % 5;
        int row = digit / 5;
        int x = key_xs[col];
        int y = row_ys[row];

        // Key is green if this digit appears in current zip
        bool used = info->zip_code.find(to_string(digit)) != string::npos;

        if (used) {
            auto key = create_green_button(to_string(digit),
                Rect(x, y, key_sz, key_sz),
                [=]() {
                    if (info->zip_code.length() < 5) {
                        info->zip_code += to_string(digit);
                        if (on_show_screen)
                            on_show_screen(create_zip_step(demo_mode, info, on_complete,
                                on_back, on_show_screen, on_leave_demo));
                    }
                });
            key->font(Font(24, Font::Weight::bold));
            key->border_radius(dt::RADIUS_SM);
            // Pin to a square (Figma keys are 40x40 -> 74x74); the digit text
            // would otherwise auto-grow the button taller than wide.
            key->min_size_hint(Size(key_sz, key_sz));
            key->resize(Size(key_sz, key_sz));
            container->add(key);
        } else {
            auto key = create_gradient_key(to_string(digit),
                Rect(x, y, key_sz, key_sz),
                [=]() {
                    if (info->zip_code.length() < 5) {
                        info->zip_code += to_string(digit);
                        if (on_show_screen)
                            on_show_screen(create_zip_step(demo_mode, info, on_complete,
                                on_back, on_show_screen, on_leave_demo));
                    }
                });
            key->min_size_hint(Size(key_sz, key_sz));
            key->resize(Size(key_sz, key_sz));
            container->add(key);
        }
    }

    // Bottom buttons (same Figma layout/shadows as the Gender step):
    // Reset @x=26 text-only, Skip @x=293 icon, Continue @x=556 blue, y=397.
    const Rect back_r(26, 397, 156, 61);
    const Rect skip_r(293, 397, 156, 61);
    const Rect cont_r(556, 397, 217, 61);

    container->add(make_shared<SoftShadow>(back_r, dt::RADIUS_XS));
    auto btn_reset = ui::create_outlined_button("Reset", back_r,
        [=]() {
            info->zip_code.clear();
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    btn_reset->font(Font(22, Font::Weight::bold));
    // Match the Skip/Continue shape: 1px border, RADIUS_XS, off-white bg
    // (create_outlined_button defaults to a thicker 2px border + RADIUS_MD).
    btn_reset->border(1);
    btn_reset->border_radius(dt::RADIUS_XS);
    btn_reset->color(Palette::ColorId::button_bg, Color(0xFA, 0xFA, 0xFA));
    // Pin to the rect so it doesn't grow taller than Skip/Continue (61).
    btn_reset->min_size_hint(Size(back_r.width(), back_r.height()));
    btn_reset->resize(Size(back_r.width(), back_r.height()));
    container->add(btn_reset);

    container->add(make_shared<SoftShadow>(skip_r, dt::RADIUS_XS));
    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip", skip_r,
        [=]() {
            info->zip_code.clear();  // skip => no value collected
            if (on_show_screen)
                on_show_screen(create_summary_step(demo_mode, info, on_complete,
                    [=]() {
                        if (on_show_screen)
                            on_show_screen(create_zip_step(demo_mode, info, on_complete,
                                on_back, on_show_screen, on_leave_demo));
                    },
                    on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    // Continue gated on the ZIP field being non-empty (task 5).
    container->add(make_shared<SoftShadow>(cont_r, dt::RADIUS_XS));
    auto btn_continue = make_continue_btn(
        !info->zip_code.empty(), demo_mode, cont_r,
        [=]() {
            if (on_show_screen)
                on_show_screen(create_summary_step(demo_mode, info, on_complete,
                    [=]() {
                        if (on_show_screen)
                            on_show_screen(create_zip_step(demo_mode, info, on_complete,
                                on_back, on_show_screen, on_leave_demo));
                    },
                    on_show_screen, on_leave_demo));
        });
    container->add(btn_continue);

    return container;
}

// ── Step 4: Summary (Gender / Age / ZIP review before GO) ──────────────────
static shared_ptr<Widget> create_summary_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back_to_zip,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    // Summary uses the "all steps complete" header: three pill tabs + a
    // full-width green bar (Figma 2009:962).
    auto container = make_patient_step(2, demo_mode, on_leave_demo,
                                       nullptr, /*as_pills=*/true);

    // Borderless review block (Figma Group 225): label + bold value rows
    // with two thin divider lines, no surrounding card. Skipped fields show
    // "-" so the summary honestly reflects what the user provided.
    struct SummaryRow { const char* label; string value; int y; };
    SummaryRow rows[] = {
        {"Gender :",  info->gender.empty()   ? "-" : info->gender,    176},
        {"Age:",      info->age > 0          ? to_string(info->age) : "-", 232},
        {"ZIP Code:", info->zip_code.empty() ? "-" : info->zip_code,  288},
    };

    // Figma 2009:968: a single LEFT-aligned block — labels all start at the
    // same x (263) and values tab-align at the same x (417). (Not colon-
    // aligned/right-aligned labels.) Font 14pt * SCALE.
    const int lbl_x = 263, lbl_w = 170;
    const int val_x = 417, val_w = 200;
    for (auto& r : rows) {
        auto lbl = make_shared<Label>(r.label,
            Rect(lbl_x, r.y, lbl_w, 44),
            AlignFlag::center_vertical | AlignFlag::left);
        lbl->font(Font(26, Font::Weight::normal));
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(lbl);

        auto val = make_shared<Label>(r.value,
            Rect(val_x, r.y, val_w, 44),
            AlignFlag::center_vertical | AlignFlag::left);
        val->font(Font(26, Font::Weight::bold));
        val->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(val);
    }

    // Two divider lines (Figma Line 2 / Line 3), black @ 10%, full block width.
    for (int dy : {222, 281}) {
        auto div = make_shared<Frame>(Rect(166, dy, 467, 1));
        div->fill_flags({Theme::FillFlag::blend});
        div->color(Palette::ColorId::bg, Color(0, 0, 0, 26));
        div->border(0);
        container->add(div);
    }

    // Back + GO sit together in the centre-bottom (Figma: Back x=233, GO
    // x=424, both 156x61, y=213*1.836=391). Same soft shadow as the other steps.
    const Rect back_r(233, 391, 156, 61);
    const Rect go_r(424, 391, 156, 61);

    container->add(make_shared<SoftShadow>(back_r, dt::RADIUS_XS));
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-sum", kArrowBackSvg, "  Back", back_r,
        [=]() { if (on_back_to_zip) on_back_to_zip(); });
    container->add(btn_back);

    // GO button — flow accent (green in real flow, blue in demo)
    const Color go_accent = flow_accent(demo_mode);
    container->add(make_shared<SoftShadow>(go_r, dt::RADIUS_XS));
    auto btn_go = make_shared<Button>("GO", go_r);
    btn_go->color(Palette::ColorId::button_bg, go_accent);
    btn_go->color(Palette::ColorId::button_text, dt::kWhite);
    btn_go->color(Palette::ColorId::border, go_accent);
    btn_go->border(0);
    btn_go->border_radius(dt::RADIUS_XS);
    btn_go->font(Font(dt::FONT_BUTTON + 4, Font::Weight::bold));
    // Pin to the rect so the "GO" text doesn't auto-grow the button to 78 tall.
    btn_go->min_size_hint(Size(go_r.width(), go_r.height()));
    btn_go->resize(Size(go_r.width(), go_r.height()));
    btn_go->on_click([=](Event&) {
        if (on_complete) on_complete(*info);
    });
    container->add(btn_go);

    return container;
}
