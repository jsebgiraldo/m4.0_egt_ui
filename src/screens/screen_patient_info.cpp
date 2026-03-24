#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_patient_info.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

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

// Male person silhouette (pants) — kTextPrimary #646469
static const char* kMaleSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 2c1.1 0 2 .9 2 2s-.9 2-2 2-2-.9-2-2 .9-2 2-2zm6 7h-5V8h-2v1H6l2 7h2v4h2v-4h2l2-7z" fill="#646469"/>
</svg>)svg";

// Female person silhouette (skirt) — kTextPrimary #646469
static const char* kFemaleSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 2c1.1 0 2 .9 2 2s-.9 2-2 2-2-.9-2-2 .9-2 2-2zm6 7H6l2 8h3v5h2v-5h3l2-8z" fill="#646469"/>
</svg>)svg";

// Arrow left — Back button (#646469)
static const char* kArrowBackSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z" fill="#646469"/>
</svg>)svg";

// Skip-next — Skip button (#646469)
static const char* kSkipNextSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z" fill="#646469"/>
</svg>)svg";

// Arrow right (white) — Continue button on green bg
static const char* kArrowFwdSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 4l-1.41 1.41L16.17 11H4v2h12.17l-5.58 5.59L12 20l8-8z" fill="#FFFFFF"/>
</svg>)svg";

// Refresh/reset — Reset button (#646469)
static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646469"/>
</svg>)svg";

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

// ── Icon button helpers ───────────────────────────────────────────────────────
// Outlined button with a leading icon (gray border, kTextPrimary text)
static shared_ptr<ImageButton> make_icon_outlined_btn(
    const char* icon_name, const char* icon_svg,
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto ico = load_svg_icon(icon_name, icon_svg, 22);
    auto btn = make_shared<ImageButton>(ico, text, rect, AlignFlag::center);
    btn->image_align(AlignFlag::left | AlignFlag::center_vertical);
    btn->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
    btn->color(Palette::ColorId::button_bg, dt::kWhite);
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->color(Palette::ColorId::border, dt::kGrayLight);
    btn->border(2);
    btn->border_radius(dt::RADIUS_MD);
    if (on_click) btn->on_click([on_click](Event&) { on_click(); });
    return btn;
}

// Green filled button with a leading icon (white icon/text on kGreen bg)
static shared_ptr<ImageButton> make_icon_green_btn(
    const char* icon_name, const char* icon_svg,
    const string& text, const Rect& rect, function<void()> on_click)
{
    auto ico = load_svg_icon(icon_name, icon_svg, 22);
    auto btn = make_shared<ImageButton>(ico, text, rect, AlignFlag::center);
    btn->image_align(AlignFlag::left | AlignFlag::center_vertical);
    btn->font(Font(dt::FONT_BUTTON, Font::Weight::bold));
    btn->color(Palette::ColorId::button_bg, dt::kGreen);
    btn->color(Palette::ColorId::button_text, dt::kWhite);
    btn->border(0);
    btn->border_radius(dt::RADIUS_SM);
    if (on_click) btn->on_click([on_click](Event&) { on_click(); });
    return btn;
}

// ── Common header + tab bar (Figma layout) ──────────────────────────────────
// step: 0=Gender, 1=Age, 2=ZIP
static shared_ptr<Frame> make_patient_step(
    int step, bool demo_mode, function<void()> on_leave_demo)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo @(4,7, 167×104)
    auto logo = ui::create_logo(4, 7, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Title "Please Enter Patient Information" @(244,17)
    auto title = make_shared<Label>("Please Enter Patient Information",
        Rect(244, 17, 350, 46),
        AlignFlag::center_vertical | AlignFlag::left);
    title->font(Font(22, Font::Weight::normal));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // User icon placeholder (gray circle) @(598,26, 28×28)
    auto user_icon = make_shared<Frame>(Rect(598, 26, 28, 28));
    user_icon->fill_flags({Theme::FillFlag::blend});
    user_icon->color(Palette::ColorId::bg, Color(0xD9, 0xD9, 0xD9));
    user_icon->border(0);
    user_icon->border_radius(14);
    container->add(user_icon);

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

    // Demo badge
    if (demo_mode && on_leave_demo) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 220, 65, on_leave_demo);
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

// ── Step 1: Gender (toggle switch) ──────────────────────────────────────────
// Figma: Male label + toggle track (pill) + Female label, centered at y≈235
static shared_ptr<Widget> create_gender_step(
    bool demo_mode,
    shared_ptr<PatientInfo> info,
    function<void(const PatientInfo&)> on_complete,
    function<void()> on_back,
    function<void(shared_ptr<Widget>)> on_show_screen,
    function<void()> on_leave_demo)
{
    auto container = make_patient_step(0, demo_mode, on_leave_demo);

    bool is_female = (info->gender == "Female");
    if (info->gender.empty()) { info->gender = "Male"; is_female = false; }

    // "Male" label @(198, 235) — tappable
    auto male_lbl = make_shared<Label>("Male",
        Rect(160, 235, 130, 37),
        AlignFlag::center_vertical | AlignFlag::right);
    male_lbl->font(Font(22, Font::Weight::bold));
    male_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(male_lbl);

    // Male person icon (just left of "Male" text)
    {
        auto ico = load_svg_icon("male-pi", kMaleSvg, 28);
        if (!ico.empty()) {
            auto lbl = make_shared<ImageLabel>(ico, "", Rect(188, 237, 28, 28));
            container->add(lbl);
        }
    }

    // Toggle track (pill) @(350, 233, 102×37) r=18
    auto track = make_shared<Frame>(Rect(350, 233, 102, 37));
    track->fill_flags({Theme::FillFlag::blend});
    track->color(Palette::ColorId::bg, dt::kBgWhite);
    track->border(2);
    track->color(Palette::ColorId::border, dt::kGreen);
    track->border_radius(18);
    container->add(track);

    // Toggle handle (circle 34×34) — left for Male, right for Female
    const int handle_sz = 34;
    const int handle_y = 1;
    const int handle_left_x = 2;
    const int handle_right_x = 66;
    auto handle = make_shared<Frame>(
        Rect(is_female ? handle_right_x : handle_left_x, handle_y, handle_sz, handle_sz));
    handle->fill_flags({Theme::FillFlag::blend});
    handle->color(Palette::ColorId::bg, dt::kGreen);
    handle->border(0);
    handle->border_radius(handle_sz / 2);
    track->add(handle);

    // "Female" label @(490, 235) — tappable
    auto female_lbl = make_shared<Label>("Female",
        Rect(490, 235, 150, 37),
        AlignFlag::center_vertical | AlignFlag::left);
    female_lbl->font(Font(22, Font::Weight::bold));
    female_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(female_lbl);

    // Female person icon (just right of "Female" text)
    {
        auto ico = load_svg_icon("female-pi", kFemaleSvg, 28);
        if (!ico.empty()) {
            auto lbl = make_shared<ImageLabel>(ico, "", Rect(568, 237, 28, 28));
            container->add(lbl);
        }
    }

    // Click on Male label → select Male, rebuild
    auto select_and_rebuild = [=](bool female) {
        info->gender = female ? "Female" : "Male";
        if (on_show_screen)
            on_show_screen(create_gender_step(demo_mode, info, on_complete,
                on_back, on_show_screen, on_leave_demo));
    };

    male_lbl->on_event([=](Event& event) {
        if (event.id() == EventId::pointer_click) { select_and_rebuild(false); return; }
    });
    female_lbl->on_event([=](Event& event) {
        if (event.id() == EventId::pointer_click) { select_and_rebuild(true); return; }
    });
    track->on_event([=](Event& event) {
        if (event.id() == EventId::pointer_click) { select_and_rebuild(!is_female); return; }
    });

    // Bottom buttons: Back @(26,400), Skip @(293,400), Continue @(556,400)
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "Back",
        Rect(26, 400, 172, 61), on_back);
    container->add(btn_back);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "Skip",
        Rect(293, 400, 156, 61),
        [=]() {
            if (on_show_screen)
                on_show_screen(create_age_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    auto btn_continue = make_icon_green_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "Continue",
        Rect(556, 400, 217, 61),
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
    auto container = make_patient_step(1, demo_mode, on_leave_demo);

    // Age ranges from Figma
    struct AgeRange { const char* label; int low; };
    const AgeRange ranges[] = {
        {"1\n|\n5", 1}, {"6\n|\n12", 6}, {"13\n|\n18", 13},
        {"19\n|\n29", 19}, {"30\n|\n49", 30}, {"50\n|\n69", 50},
    };
    const int num_ranges = 6;

    // Find pre-selected index
    int sel = -1;
    for (int i = 0; i < num_ranges; i++) {
        if (info->age == ranges[i].low) { sel = i; break; }
    }

    // Picker container box @(102, 159, 613×191)
    const int box_x = 102, box_y = 159, box_w = 613, box_h = 191;
    auto picker_box = make_shared<Frame>(Rect(box_x, box_y, box_w, box_h));
    picker_box->fill_flags({Theme::FillFlag::blend});
    picker_box->color(Palette::ColorId::bg, dt::kBgWhite);
    picker_box->border(1);
    picker_box->color(Palette::ColorId::border, dt::kGrayLight);
    picker_box->border_radius(4);
    container->add(picker_box);

    // Range items inside picker
    const int item_w = 80, item_h = 140;
    const int item_gap = 10;
    const int total_w = num_ranges * item_w + (num_ranges - 1) * item_gap;
    const int items_x = (box_w - total_w) / 2;
    const int items_y = (box_h - item_h) / 2;

    for (int i = 0; i < num_ranges; i++) {
        int x = items_x + i * (item_w + item_gap);
        bool is_sel = (i == sel);

        auto rf = make_shared<Frame>(Rect(x, items_y, item_w, item_h));
        rf->fill_flags({Theme::FillFlag::blend});
        rf->color(Palette::ColorId::bg, is_sel ? dt::kGreen : Color(0, 0, 0, 0));
        rf->border(0);
        rf->border_radius(4);

        auto lbl = make_shared<Label>(ranges[i].label,
            Rect(0, 0, item_w, item_h), AlignFlag::center);
        lbl->font(Font(20, Font::Weight::bold));
        lbl->color(Palette::ColorId::label_text, is_sel ? dt::kWhite : dt::kTextPrimary);
        rf->add(lbl);

        int low = ranges[i].low;
        rf->on_event([=](Event& event) {
            if (event.id() == EventId::pointer_click) {
                info->age = low;
                if (on_show_screen)
                    on_show_screen(create_age_step(demo_mode, info, on_complete,
                        on_back, on_show_screen, on_leave_demo));
            }
        });
        picker_box->add(rf);
    }

    // Left/right arrow decoration
    auto left_arr = make_shared<Label>("\u25C0",
        Rect(55, box_y + box_h / 2 - 30, 40, 60), AlignFlag::center);
    left_arr->font(Font(28));
    left_arr->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(left_arr);

    auto right_arr = make_shared<Label>("\u25B6",
        Rect(722, box_y + box_h / 2 - 30, 40, 60), AlignFlag::center);
    right_arr->font(Font(28));
    right_arr->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(right_arr);

    // Bottom buttons
    auto btn_back = make_icon_outlined_btn(
        "arrow-back-pi", kArrowBackSvg, "Back",
        Rect(26, 400, 172, 61),
        [=]() {
            if (on_show_screen)
                on_show_screen(create_gender_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_back);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "Skip",
        Rect(293, 400, 156, 61),
        [=]() {
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_skip);

    auto btn_continue = make_icon_green_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "Continue",
        Rect(556, 400, 217, 61),
        [=]() {
            if (info->age > 0) {
                if (on_show_screen)
                    on_show_screen(create_zip_step(demo_mode, info, on_complete,
                        on_back, on_show_screen, on_leave_demo));
            }
        });
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
    auto container = make_patient_step(2, demo_mode, on_leave_demo);

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

    // Bottom buttons: Reset @(26,400), Skip @(293,400), Continue @(556,400)
    auto btn_reset = make_icon_outlined_btn(
        "refresh-pi", kRefreshSvg, "Reset",
        Rect(26, 400, 172, 61),
        [=]() {
            info->zip_code.clear();
            if (on_show_screen)
                on_show_screen(create_zip_step(demo_mode, info, on_complete,
                    on_back, on_show_screen, on_leave_demo));
        });
    container->add(btn_reset);

    auto btn_skip = make_icon_outlined_btn(
        "skip-next-pi", kSkipNextSvg, "Skip",
        Rect(293, 400, 156, 61),
        [=]() {
            if (on_complete) on_complete(*info);
        });
    container->add(btn_skip);

    auto btn_continue = make_icon_green_btn(
        "arrow-fwd-pi", kArrowFwdSvg, "Continue",
        Rect(556, 400, 217, 61),
        [=]() {
            if (info->zip_code.length() >= 3 && on_show_screen)
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

    // Row: label + bold value
    struct SummaryRow { const char* label; string value; int y; };
    SummaryRow rows[] = {
        {"Gender :",  info->gender,
                      165},
        {"Age:",      info->age > 0 ? to_string(info->age) : "-",
                      225},
        {"ZIP Code:", info->zip_code.empty() ? "-" : info->zip_code,
                      285},
    };

    for (auto& r : rows) {
        auto lbl = make_shared<Label>(r.label,
            Rect(160, r.y, 180, 40),
            AlignFlag::center_vertical | AlignFlag::right);
        lbl->font(Font(20, Font::Weight::normal));
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(lbl);

        auto val = make_shared<Label>(r.value,
            Rect(360, r.y, 220, 40),
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
        "arrow-back-sum", kArrowBackSvg, "Back",
        Rect(26, 400, 172, 61),
        [=]() { if (on_back_to_zip) on_back_to_zip(); });
    container->add(btn_back);

    // GO button (green, bold, no icon)
    auto btn_go = make_shared<Button>("GO", Rect(556, 400, 217, 61));
    btn_go->color(Palette::ColorId::button_bg, dt::kGreen);
    btn_go->color(Palette::ColorId::button_text, dt::kWhite);
    btn_go->color(Palette::ColorId::border, dt::kGreen);
    btn_go->border(0);
    btn_go->border_radius(dt::RADIUS_SM);
    btn_go->font(Font(dt::FONT_BUTTON + 4, Font::Weight::bold));
    btn_go->on_click([=](Event&) {
        if (on_complete) on_complete(*info);
    });
    container->add(btn_go);

    return container;
}
