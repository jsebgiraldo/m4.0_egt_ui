#include "screen_demo_info.h"
#include "../ui/design_tokens.h"
#include "../ui/components.h"
#include "../ui/palette.h"

using namespace egt;
using namespace std;

namespace {

// Clickable image-button: an ImageLabel inside a Frame. The PNG already
// contains background, icon, and text rendered by Figma at scale=2, so the
// helper just sizes it to the device-space rect and forwards clicks.
shared_ptr<Frame> make_image_button(
    const string& png_path,
    const Rect& rect,
    function<void()> on_click)
{
    auto wrap = make_shared<Frame>(rect);
    wrap->fill_flags({});  // transparent so the PNG corners blend in

    try {
        // Probe the PNG so we can pick scale factors that fit the rect
        // without distortion (both axes use the smaller of the two ratios).
        auto probe = Image(("file:" + png_path).c_str());
        const float sw = static_cast<float>(probe.width());
        const float sh = static_cast<float>(probe.height());
        const float hs = static_cast<float>(rect.width())  / sw;
        const float vs = static_cast<float>(rect.height()) / sh;
        const float s  = std::min(hs, vs);
        auto img = Image(("file:" + png_path).c_str(), s, s);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, rect.width(), rect.height()));
        wrap->add(lbl);
    } catch (const std::exception& e) {
        printf("[DEMO_INFO] image %s missing: %s\n", png_path.c_str(), e.what());
        fflush(stdout);
    }

    wrap->on_event([on_click](Event& e) {
        if (e.id() == EventId::pointer_click && on_click) on_click();
    });

    return wrap;
}

} // namespace

// ── DEMO INFO screen (Figma node 84:608, "Demonstration Mode") ──────────────
// Figma frame: 432x261 at (89, 499) -> 800x480 device-px (SCALE = 1.852).
// Bayron-personal duplicate file key: OYaZtgoJHMtHpIO4vFYJ6V.
shared_ptr<Widget> create_demo_info_screen(
    function<void()> on_continue,
    function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (Figma 37:1831, 90x56 @(91,503) -> frame-local (2,4)) ─────────
    auto logo = ui::create_logo(4, 7, 167, 104);
    container->add(logo);

    // ── "Demonstration Mode" title (Figma 37:1833, 141x18 @(236,533)) ──────
    auto title = make_shared<Label>("Demonstration Mode",
        Rect(272, 55, 280, 40), AlignFlag::left);
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── DEMO MODE badge (Figma 67:761, 79x44 @(442,505)) ───────────────────
    // 2-line stacked so it never clips.
    auto demo_l1 = make_shared<Label>("DEMO",
        Rect(dt::SCREEN_W - 140, 14, 124, 30), AlignFlag::center);
    demo_l1->font(Font(22, Font::Weight::bold));
    demo_l1->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(demo_l1);

    auto demo_l2 = make_shared<Label>("MODE",
        Rect(dt::SCREEN_W - 140, 44, 124, 30), AlignFlag::center);
    demo_l2->font(Font(22, Font::Weight::bold));
    demo_l2->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(demo_l2);

    // ── Exit-demo button (Figma 84:565 "bt leave", 60x24 @(452,551)) ───────
    // Top-right under the DEMO MODE badge. Returns to Home (same as Back).
    // Figma frame-local (363, 52) -> device (672, 96), size 60x24 -> 111x44.
    auto btn_exit = make_image_button(
        "assets/figma/images/demo-info-btn-exit.png",
        Rect(672, 96, 111, 44),
        on_back);
    container->add(btn_exit);

    // ── Green progress bar (Figma 37:1836, 432x3 @y=578 -> y=146) ──────────
    auto green_bar = make_shared<Frame>(Rect(0, 146, dt::SCREEN_W, 6));
    green_bar->fill_flags({Theme::FillFlag::blend});
    green_bar->color(Palette::ColorId::bg, dt::kGreen);
    green_bar->border(0);
    container->add(green_bar);

    // ── Divider line (Figma 37:1835, 432x1 @y=579 -> y=148) ────────────────
    auto divider = make_shared<Frame>(Rect(0, 152, dt::SCREEN_W, 2));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    // ── "TRAINING ONLY" - large, blue, centred ─────────────────────────────
    auto training = make_shared<Label>("TRAINING ONLY",
        Rect(0, 178, dt::SCREEN_W, 50), AlignFlag::center);
    training->font(Font(36, Font::Weight::bold));
    training->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(training);

    // ── Body - dark, normal weight ─────────────────────────────────────────
    auto body = make_shared<Label>(
        "The Demo Mode has a lower temperature\n"
        "and a lower fan speed.",
        Rect(0, 248, dt::SCREEN_W, 60), AlignFlag::center);
    body->font(Font(dt::FONT_SUBTITLE, Font::Weight::normal));
    body->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(body);

    // ── "Do Not Use For Treatments !" - blue, bold, lower ──────────────────
    auto warn = make_shared<Label>("Do Not Use For Treatments !",
        Rect(0, 322, dt::SCREEN_W, 36), AlignFlag::center);
    warn->font(Font(dt::FONT_SUBTITLE, Font::Weight::bold));
    warn->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(warn);

    // ── Back button (Figma 81:555 "bt EXIT", 84x33 @(103,715)) ─────────────
    // Frame-local (14, 216) -> device (26, 400). The PNG carries the gray
    // circle, left chevron, and "Back" text already; we just place it.
    auto btn_back = make_image_button(
        "assets/figma/images/demo-info-btn-back.png",
        Rect(26, 400, 156, 61),
        on_back);
    container->add(btn_back);

    // ── Continue button (Figma 81:505 "bt continue", 117x33 @(393,715)) ────
    // Frame-local (304, 216) -> device (563, 400). PNG has cyan bg, white
    // circle with right chevron, and "Continue" text.
    auto btn_continue = make_image_button(
        "assets/figma/images/demo-info-btn-continue.png",
        Rect(563, 400, 217, 61),
        on_continue);
    container->add(btn_continue);

    return container;
}
