#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_patient_info.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <cmath>
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

// Disc + arrow-left — Back / Reset glyph: light gray disc, dark gray glyph.
// Same proportions as Male/Female: glyph translate(3,3) scale(0.75) — fills
// the disc generously (Material Symbols arrow_back).
static const char* kArrowBackSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#E8E8E8"/>
  <g transform="translate(3,3) scale(0.75)">
    <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z" fill="#646469"/>
  </g>
</svg>)svg";

// Disc + Skip glyph — Figma "Vector 3980" (curved-arrow loop + chevron).
// Stroked path, no rotation (the 180° in Figma is already baked into how
// we read the path here so the chevron points down-right as in the design).
static const char* kSkipNextSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#E8E8E8"/>
  <g transform="translate(3.04,3.36) scale(0.32)"
     fill="none" stroke="#646469" stroke-width="5"
     stroke-linecap="round" stroke-linejoin="round">
    <path d="M4.72 49.28 C4.72 22.05 2.70 4.66 22.51 4.66 C42.32 4.66 37.13 31.89 37.13 49.28 M22.51 33.84 L37.13 49.28 L53.34 29.95"/>
  </g>
</svg>)svg";

// Disc + arrow-right — Continue glyph on green bg: white translucent disc,
// white glyph (Material Symbols arrow_forward)
static const char* kArrowFwdSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="12" cy="12" r="12" fill="#FFFFFF" fill-opacity="0.30"/>
  <g transform="translate(3,3) scale(0.75)">
    <path d="M12 4l-1.41 1.41L16.17 11H4v2h12.17l-5.58 5.59L12 20l8-8z" fill="#FFFFFF"/>
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

// Figma Group 231: Patient Information — 3 sub-screens (Gender, Age, ZIP)
// Canvas: 432×261, screen: 800×480 → scale ≈ 1.852

// ── Wheel gradient backdrop ─────────────────────────────────────────────────
// Same widget the Technician Log-in picker uses (screen_login_v2): vertical
// gradient from medium-gray edges to a pure-white plateau over the selected
// slot, with rounded corners. Combined with per-slot alpha on the labels it
// fakes the "cylinder rolling behind a curved window" look.
class AgeWheelGradient : public egt::Widget {
public:
    AgeWheelGradient(const egt::Rect& r, float plateau_top, float plateau_bot,
                     float radius = 6.0f)
        : egt::Widget(r), m_radius(radius),
          m_plateau_top(plateau_top), m_plateau_bot(plateau_bot)
    {
        fill_flags({egt::Theme::FillFlag::blend});
        border(0);
    }

    void draw(egt::Painter& painter, const egt::Rect&) override
    {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());

        const egt::Color edge {212, 212, 212};
        const egt::Color white{255, 255, 255};
        egt::Pattern grad(egt::Pattern::StepArray{
            {0.0f,            edge},
            {m_plateau_top,   white},
            {m_plateau_bot,   white},
            {1.0f,            edge}
        }, egt::Point(static_cast<int>(x), static_cast<int>(y)),
           egt::Point(static_cast<int>(x), static_cast<int>(y + h)));

        const float r = m_radius;
        const auto PI = static_cast<float>(M_PI);
        painter.draw(egt::PointF(x + r, y));
        painter.line(egt::PointF(x + w - r, y));
        painter.draw(egt::Arc(egt::PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
        painter.line(egt::PointF(x + w, y + h - r));
        painter.draw(egt::Arc(egt::PointF(x + w - r, y + h - r),   r, 0.0f,   PI / 2));
        painter.line(egt::PointF(x + r, y + h));
        painter.draw(egt::Arc(egt::PointF(x + r, y + h - r),       r, PI / 2, PI));
        painter.line(egt::PointF(x, y + r));
        painter.draw(egt::Arc(egt::PointF(x + r, y + r),           r, PI, 3 * PI / 2));
        painter.set(grad);
        painter.fill();
    }

private:
    float m_radius;
    float m_plateau_top;
    float m_plateau_bot;
};

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

// ── Icon button helpers ───────────────────────────────────────────────────────
// Outlined button: off-white bg, 1 px gray border for separation from page bg
static shared_ptr<ImageButton> make_icon_outlined_btn(
    const char* icon_name, const char* icon_svg,
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto ico = load_svg_icon(icon_name, icon_svg, 44);
    auto btn = make_shared<ImageButton>(ico, text, rect, AlignFlag::center);
    btn->image_align(AlignFlag::left | AlignFlag::center_vertical);
    btn->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
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
    btn->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
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
    function<void(int)> nav_to_step = nullptr)
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
        auto img = Image(("file:" + ui::asset_path("patient-title-profile")).c_str());
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

    for (int i = 0; i <= step; i++) {
        auto tab = make_shared<Label>(tab_names[i],
            Rect(tab_x[i], 70, tab_w[i], 30),
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

    // Divider line @(0,111, 800×2)
    auto divider = make_shared<Frame>(Rect(0, 111, 800, 2));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    // Green indicator bar under active tab @(green_bar_x, 109, 133×6)
    auto indicator = make_shared<Frame>(Rect(green_bar_x[step], 109, 133, 6));
    indicator->fill_flags({Theme::FillFlag::blend});
    indicator->color(Palette::ColorId::bg, dt::kGreen);
    indicator->border(0);
    container->add(indicator);

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

    const int card_w = 176, card_h = 150, gap = 30;
    const int start_x = (800 - 2 * card_w - gap) / 2;
    const int card_y = 175;
    const int icon_sz = 66, icon_y = 18;

    auto build_card = [&](bool female_card) {
        const bool selected = has_gender && (female_card == is_female);
        const int x = start_x + (female_card ? (card_w + gap) : 0);

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
        const std::string icon_path = ui::asset_path(
            female_card ? "patient-female-icon" : "patient-male-icon");
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

    // Bottom buttons: Back @(42,400) Skip @(292,400) Continue @(541,400)
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "  Back",
        Rect(42, 387, 156, 61), on_back);
    container->add(btn_back);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip",
        Rect(292, 387, 156, 61),
        [=]() {
            info->gender.clear();   // skip => no value collected
            if (on_show_screen)
                on_show_screen(create_age_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    // Continue gated on a gender being selected (task 5).
    auto btn_continue = make_continue_btn(
        !info->gender.empty(), demo_mode,
        Rect(541, 387, 217, 61),
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

    // FIXED slot layout up front so the gradient's white plateau lands exactly
    // on the centre row (same trick as the Technician Log-in picker).
    const int slots_top       = chevron_h + padding;
    const int center_slot_idx = n_slots / 2;
    const int sel_top_y     = slots_top + center_slot_idx * slot_h;
    const float plateau_top = static_cast<float>(sel_top_y) / box_h;
    const float plateau_bot = static_cast<float>(sel_top_y + slot_h) / box_h;
    auto picker_bg = make_shared<AgeWheelGradient>(
        Rect(box_x, box_y, box_w, box_h),
        plateau_top, plateau_bot, 6.0f);
    container->add(picker_bg);

    // Inner Frame is transparent so the gradient shows through.
    auto picker_box = make_shared<Frame>(Rect(box_x, box_y, box_w, box_h));
    picker_box->fill_flags({});
    picker_box->border(0);
    container->add(picker_box);

    // Up / down PNG arrows from Figma \u2014 same assets the login picker uses.
    auto load_arrow = [&](const std::string& png_path, int y) {
        auto wrap = make_shared<Frame>(Rect(0, y, box_w, chevron_h));
        wrap->fill_flags({});
        try {
            auto img = Image(("file:" + png_path).c_str());
            auto lbl = make_shared<ImageLabel>(img);
            lbl->autoresize(false);
            lbl->border(0); lbl->padding(0); lbl->margin(0);
            lbl->fill_flags({});
            lbl->image_align(AlignFlag::center);
            lbl->box(Rect(0, 0, box_w, chevron_h));
            wrap->add(lbl);
        } catch (...) { /* fall back: no arrow */ }
        return wrap;
    };
    picker_box->add(load_arrow(ui::asset_path("wheel-arrow-up"), 4));
    picker_box->add(load_arrow(ui::asset_path("wheel-arrow-down"),
                               box_h - chevron_h - 4));

    // FIXED slot frames render the wheel. They never move; a transparent
    // vertical Slider on top drives `sel` with live_update — same pattern
    // we used for brightness, which is the only way EGT emits continuous
    // value-change events during a drag.
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
    // Per-slot alpha makes the top/bottom rows dissolve into the gradient
    // edges (same effect as the login picker). 5-slot symmetric ramp:
    //   k=0 / k=4: very faint   (alpha 60)
    //   k=1 / k=3: half visible (alpha 180)
    //   k=2:       selected     (alpha 255, bold)
    auto redraw = [=](int state) {
        for (int k = 0; k < n_slots; k++) {
            const int idx = state + (k - center_slot_idx);
            if (idx < 0 || idx >= num_ranges) {
                (*slot_labels)[k]->text("");
                continue;
            }
            const bool is_sel = (k == center_slot_idx);
            (*slot_labels)[k]->text(ranges[idx].label);
            (*slot_labels)[k]->font(Font(
                is_sel ? 28 : 22,
                is_sel ? Font::Weight::bold : Font::Weight::normal));

            int alpha;
            switch (k) {
                case 0:  alpha =  60; break;
                case 1:  alpha = 180; break;
                case 2:  alpha = 255; break;
                case 3:  alpha = 180; break;
                default: alpha =  60; break;   // k == 4
            }
            const auto base = dt::kTextPrimary;
            const Color c(base.red(), base.green(), base.blue(),
                          static_cast<uint8_t>(alpha));
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

    // Bottom buttons
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "  Back",
        Rect(42, 387, 156, 61),
        [=]() {
            if (on_show_screen)
                on_show_screen(create_gender_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_back);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip",
        Rect(292, 387, 156, 61),
        [=]() {
            info->age = 0;  // skip => no value collected
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    auto btn_continue = make_icon_filled_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "  Continue",
        Rect(541, 387, 217, 61),
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
        Rect(0, 125, 800, 40), AlignFlag::center);
    zip_display->font(Font(28, Font::Weight::bold));
    zip_display->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(zip_display);

    // Keypad: 5 cols × 2 rows, keys 74×74. Row gap (≈34 px) is now a little
    // wider than the column gap (≈22 px) so the two rows don't look cramped.
    const int key_sz = 74;
    const int key_xs[] = {178, 274, 370, 467, 561};
    const int row_ys[] = {172, 280};

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
            container->add(key);
        } else {
            auto key = ui::create_outlined_button(to_string(digit),
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
            container->add(key);
        }
    }

    // Bottom buttons (Figma Group 233 ZIP step — no Back, blue Continue):
    // Reset @(42,380) text-only, Skip @(292,380) icon, Continue @(541,380) blue
    auto btn_reset = ui::create_outlined_button("Reset",
        Rect(42, 387, 156, 61),
        [=]() {
            info->zip_code.clear();
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    btn_reset->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
    container->add(btn_reset);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "  Skip",
        Rect(292, 387, 156, 61),
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

    // Continue gated on all 5 ZIP digits being entered (task 5).
    auto btn_continue = make_continue_btn(
        info->zip_code.length() == 5, demo_mode,
        Rect(541, 387, 217, 61),
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
    auto container = make_patient_step(2, demo_mode, on_leave_demo);

    // Summary card background
    auto card = make_shared<Frame>(Rect(90, 145, 620, 215));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kBgWhite);
    card->border(1);
    card->color(Palette::ColorId::border, dt::kGrayLight);
    card->border_radius(dt::RADIUS_MD);
    container->add(card);

    // Row: label + bold value. Skipped fields show "-" so the summary
    // honestly reflects what the user actually provided.
    struct SummaryRow { const char* label; string value; int y; };
    SummaryRow rows[] = {
        {"Gender :",  info->gender.empty() ? "-" : info->gender,
                      165},
        {"Age:",      info->age > 0 ? to_string(info->age) : "-",
                      225},
        {"ZIP Code:", info->zip_code.empty() ? "-" : info->zip_code,
                      285},
    };

    // Centre the label+value group within the card. Card is at x=90, w=620
    // → centre x = 400. Group: label(180) + gap(20) + value(220) = 420
    // wide. Left edge: 400 - 420/2 = 190. Earlier the labels started at
    // x=160 which left ~70px on the left and ~130px on the right — visibly
    // off-centre. Now the padding is balanced inside the card.
    const int lbl_x = 190, lbl_w = 180;
    const int val_x = lbl_x + lbl_w + 20, val_w = 220;
    for (auto& r : rows) {
        auto lbl = make_shared<Label>(r.label,
            Rect(lbl_x, r.y, lbl_w, 40),
            AlignFlag::center_vertical | AlignFlag::right);
        lbl->font(Font(20, Font::Weight::normal));
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(lbl);

        auto val = make_shared<Label>(r.value,
            Rect(val_x, r.y, val_w, 40),
            AlignFlag::center_vertical | AlignFlag::left);
        val->font(Font(20, Font::Weight::bold));
        val->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(val);
    }

    // Dividers between rows
    for (int dy : {218, 278}) {
        auto div = make_shared<Frame>(Rect(100, dy, 600, 1));
        div->fill_flags({Theme::FillFlag::blend});
        div->color(Palette::ColorId::bg, dt::kGrayLight);
        div->border(0);
        container->add(div);
    }

    // Back button
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-sum", kArrowBackSvg, "  Back",
        Rect(42, 387, 156, 61),
        [=]() { if (on_back_to_zip) on_back_to_zip(); });
    container->add(btn_back);

    // GO button — flow accent (green in real flow, blue in demo)
    const Color go_accent = flow_accent(demo_mode);
    auto btn_go = make_shared<Button>("GO", Rect(541, 387, 217, 61));
    btn_go->color(Palette::ColorId::button_bg, go_accent);
    btn_go->color(Palette::ColorId::button_text, dt::kWhite);
    btn_go->color(Palette::ColorId::border, go_accent);
    btn_go->border(0);
    btn_go->border_radius(dt::RADIUS_XS);
    btn_go->font(Font(dt::FONT_BUTTON + 4, Font::Weight::bold));
    btn_go->on_click([=](Event&) {
        if (on_complete) on_complete(*info);
    });
    container->add(btn_go);

    return container;
}
