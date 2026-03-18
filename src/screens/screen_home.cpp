#include <egt/ui>
#include "screen_home.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_home_screen(
    function<void()> on_begin_treatment,
    function<void()> on_demo_mode,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (centered, upper portion) ──────────────────────────────────────
    auto logo = ui::create_logo(
        (dt::SCREEN_W - dt::LOGO_W) / 2,
        40,
        dt::LOGO_W,
        dt::LOGO_H);
    container->add(logo);

    // ── "Begin Treatment" button (large, centered) ──────────────────────────
    // Figma: two-line text "Begin" / "Treatment", ~260×135 scaled
    const int btn_w = 260;
    const int btn_h = 135;
    auto btn_begin = ui::create_filled_button(
        "Begin\nTreatment",
        Rect((dt::SCREEN_W - btn_w) / 2, 180, btn_w, btn_h),
        on_begin_treatment);
    btn_begin->font(Font(28, Font::Weight::bold));
    container->add(btn_begin);

    // ── "Demo Mode" (bottom-left, outlined with icon) ───────────────────────
    // Figma: white card button with person icon + text, ~246×96 scaled
    const int card_w = 246;
    const int card_h = 96;
    const int card_y = dt::SCREEN_H - card_h - 15;
    auto btn_demo = ui::create_outlined_button(
        "\U0001F464  Demo Mode",
        Rect(30, card_y, card_w, card_h),
        on_demo_mode);
    btn_demo->font(Font(16, Font::Weight::normal));
    container->add(btn_demo);

    // ── "Setting" (bottom-right, outlined with icon) ────────────────────────
    auto btn_setting = ui::create_outlined_button(
        "\u2699  Setting",
        Rect(dt::SCREEN_W - card_w - 30, card_y, card_w, card_h),
        on_settings);
    btn_setting->font(Font(16, Font::Weight::normal));
    container->add(btn_setting);

    return container;
}
