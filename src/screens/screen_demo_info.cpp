#include "screen_demo_info.h"
#include "../ui/design_tokens.h"
#include "../ui/components.h"
#include "../ui/palette.h"

using namespace egt;
using namespace std;

namespace {

// Clickable button rendered from a Figma PNG. The PNG was exported at
// scale=2, so its natural device size = PNG_px * (SCALE / 2) = PNG_px * 0.926.
// We size the wrap rect to that natural device size (which includes shadow
// padding), and the caller positions it so the visible button portion lands
// on the right place. Scale is uniform so the icon and text are not distorted.
shared_ptr<Frame> make_image_button(
    const string& png_path,
    const Rect& rect,
    function<void()> on_click)
{
    auto wrap = make_shared<Frame>(rect);
    wrap->fill_flags({});  // transparent so the PNG drop shadow blends

    try {
        auto probe = Image(("file:" + png_path).c_str());
        const float sw = static_cast<float>(probe.width());
        const float sh = static_cast<float>(probe.height());
        // Independent scale per axis so the PNG fills the rect exactly. The
        // caller is expected to pass a rect whose aspect matches the PNG's
        // (PNG_px * SCALE/2), so hs and vs should be equal in practice and
        // no real distortion occurs.
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
// Working file (personal-workspace duplicate): OYaZtgoJHMtHpIO4vFYJ6V.
//
// All font sizes are pulled from Figma TEXT nodes' styleOverrideTable and
// multiplied by SCALE. Title is fontSize 14 -> 26 pt, badge 20 -> 37 pt,
// TRAINING ONLY 24 -> 44 pt, body and warning 14 -> 26 pt.
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
    // Frame-local (147, 34) -> device (272, 63). fontSize 14 Bold -> 26 pt.
    auto title = make_shared<Label>("Demonstration Mode",
        Rect(272, 50, 280, 40), AlignFlag::center);
    title->font(Font("Gothic A1", 26, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── DEMO MODE badge (Figma 67:761, 79x44 @(442,505)) ───────────────────
    // Frame-local (353, 6, 79, 44) -> device (654, 11, 146, 81). The two
    // lines must fit in that 81-px vertical band so the badge does not
    // overlap the exit-demo button below (which starts at y=93). With a
    // 37 pt font, one line is ~37 px tall - placing line 1 at y=11 and
    // line 2 at y=48 puts the bottom of MODE at ~85, leaving an 8 px gap.
    auto demo_l1 = make_shared<Label>("DEMO",
        Rect(654, 11, 146, 37), AlignFlag::center);
    demo_l1->font(Font("Gothic A1", 37, Font::Weight::normal));
    demo_l1->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(demo_l1);

    auto demo_l2 = make_shared<Label>("MODE",
        Rect(654, 48, 146, 37), AlignFlag::center);
    demo_l2->font(Font("Gothic A1", 37, Font::Weight::normal));
    demo_l2->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(demo_l2);

    // ── Exit-demo button (Figma 84:565 "bt leave", 60x24 @(452,551)) ───────
    // Top-right under the DEMO MODE badge. Returns to Home (same as Back).
    // PNG 128x56 -> natural device 119x52, centred on figma button centre
    // (frame-local (393, 64) -> device (728, 119)) -> top-left (669, 93).
    auto btn_exit = make_image_button(
        "assets/figma/images/demo-info-btn-exit.png",
        Rect(669, 93, 119, 52),
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

    // ── "TRAINING ONLY" - big, cyan, regular weight (UPPERCASE in Figma) ───
    // Figma styleOverride 51: fontSize 24 Regular UPPER -> 44 pt Normal.
    auto training = make_shared<Label>("TRAINING ONLY",
        Rect(0, 170, dt::SCREEN_W, 60), AlignFlag::center);
    training->font(Font("Gothic A1", 44, Font::Weight::normal));
    training->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(training);

    // ── Body text (Figma 63:390 styleOverride 41: fontSize 14 Regular) ─────
    auto body = make_shared<Label>(
        "The Demo Mode has a lower temperature\n"
        "and a lower fan speed.",
        Rect(0, 245, dt::SCREEN_W, 70), AlignFlag::center);
    body->font(Font("Gothic A1", 26, Font::Weight::normal));
    body->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(body);

    // ── "Do Not Use For Treatments !" (styleOverride 55: 14 Regular cyan) ──
    auto warn = make_shared<Label>("Do Not Use For Treatments !",
        Rect(0, 320, dt::SCREEN_W, 36), AlignFlag::center);
    warn->font(Font("Gothic A1", 26, Font::Weight::normal));
    warn->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(warn);

    // ── Back button (Figma 81:555 "bt EXIT", 84x33 @(103,715)) ─────────────
    // Frame-local (14, 216) -> device button centre (104, 430). PNG 226x106
    // -> natural device 209x98. Top-left = centre - half-PNG = (-0.5, 381).
    // Clip x to 0; the left 1 px of shadow falls off-screen, invisible.
    auto btn_back = make_image_button(
        "assets/figma/images/demo-info-btn-back.png",
        Rect(0, 381, 209, 98),
        on_back);
    container->add(btn_back);

    // ── Continue button (Figma 81:505 "bt continue", 117x33 @(393,715)) ────
    // Frame-local (304, 216) -> device button centre (671, 430). PNG 274x106
    // -> natural device 254x98. Top-left = (544, 381).
    auto btn_continue = make_image_button(
        "assets/figma/images/demo-info-btn-continue.png",
        Rect(544, 381, 254, 98),
        on_continue);
    container->add(btn_continue);

    return container;
}
