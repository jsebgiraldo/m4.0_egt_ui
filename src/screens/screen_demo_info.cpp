#include "screen_demo_info.h"
#include "../ui/design_tokens.h"
#include "../ui/components.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_demo_info_screen(
    function<void()> on_continue,
    function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (top-left, Figma: Lice-temp logo 4, 90×56 scaled) ─────────────
    auto logo = ui::create_logo(10, 10, 166, 103);
    container->add(logo);

    // ── "Demonstration Mode" title (Figma: 141×18 @147,34 → 272,63) ────────
    auto title = make_shared<Label>("Demonstration Mode",
        Rect(272, 55, 280, 40), AlignFlag::left);
    title->font(Font(22, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title);

    // ── DEMO MODE badge (top-right, Figma: 79×44 @353,6 → 654,11) ──────────
    auto demo_label = make_shared<Label>("DEMO MODE",
        Rect(dt::SCREEN_W - 170, 10, 160, 80), AlignFlag::right);
    demo_label->font(Font(30, Font::Weight::bold));
    demo_label->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(demo_label);

    // ── Green progress bar (Figma: Rectangle 32, 432×3 @0,79 → y=146) ──────
    auto green_bar = make_shared<Frame>(Rect(0, 130, dt::SCREEN_W, 6));
    green_bar->fill_flags({Theme::FillFlag::blend});
    green_bar->color(Palette::ColorId::bg, dt::kGreen);
    green_bar->border(0);
    container->add(green_bar);

    // ── Divider line (Figma: Rectangle 31, 432×1 @0,80 → y=148) ────────────
    auto divider = make_shared<Frame>(Rect(0, 136, dt::SCREEN_W, 2));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    // ── Warning text block (Figma: 260×92 @92,101 → 170,187) ──────────────
    auto warning = make_shared<Label>(
        "Training Only\n\n"
        "The Demo Mode has a lower temperature\n"
        "and a lower fan speed.\n"
        "Do Not Use For Treatments !",
        Rect(160, 170, 480, 200));
    warning->font(Font(dt::FONT_SUBTITLE, Font::Weight::bold));
    warning->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(warning);

    // ── Back button (bottom-left, Figma: bt EXIT 84×33 → ~156×61) ───────────
    auto btn_back = ui::create_outlined_button("Back",
        Rect(30, dt::SCREEN_H - 80, 156, 61),
        on_back);
    container->add(btn_back);

    // ── Continue button (bottom-right, Figma: bt continue 117×33 → ~217×61) ─
    auto btn_continue = ui::create_filled_button("Continue",
        Rect(dt::SCREEN_W - 247, dt::SCREEN_H - 80, 217, 61),
        on_continue);
    container->add(btn_continue);

    return container;
}
