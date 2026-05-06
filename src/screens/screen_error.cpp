#include "screen_error.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_error_screen(
    ErrorLevel level,
    const string& title,
    const string& message,
    function<void()> on_primary,
    function<void()> on_secondary)
{
    // Pick banner color by severity
    Color banner_color;
    string level_label;
    switch (level) {
        case ErrorLevel::INFO:
            banner_color = dt::kAccentCyan;
            level_label = "Information";
            break;
        case ErrorLevel::WARNING:
            banner_color = dt::kOrange;
            level_label = "Warning";
            break;
        case ErrorLevel::CRITICAL:
            banner_color = dt::kRed;
            level_label = "Critical Fault";
            break;
    }

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Colored banner strip (top, Figma: full-width colored bar) ───────────
    const int banner_h = 80;
    auto banner = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, banner_color);
    banner->border(0);
    container->add(banner);

    // Level label on banner
    auto lbl_level = make_shared<Label>(level_label,
        Rect(0, 0, dt::SCREEN_W, banner_h));
    lbl_level->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    lbl_level->color(Palette::ColorId::label_text, dt::kWhite);
    banner->add(lbl_level);

    // ── Title (error name) ──────────────────────────────────────────────────
    auto lbl_title = make_shared<Label>(title,
        Rect(50, banner_h + 30, dt::SCREEN_W - 100, 40));
    lbl_title->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    lbl_title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(lbl_title);

    // ── Description message ─────────────────────────────────────────────────
    auto lbl_msg = make_shared<Label>(message,
        Rect(50, banner_h + 80, dt::SCREEN_W - 100, 120));
    lbl_msg->font(Font(dt::FONT_BODY, Font::Weight::normal));
    lbl_msg->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(lbl_msg);

    // ── Buttons based on level ──────────────────────────────────────────────
    const int btn_y = dt::SCREEN_H - 90;
    const int btn_h = 70;

    if (level == ErrorLevel::INFO && on_secondary) {
        // INFO: Resume (filled, right) + Pause (outlined, left)
        auto btn_pause = ui::create_outlined_button("Pause",
            Rect(40, btn_y, 180, btn_h),
            on_secondary);
        container->add(btn_pause);

        auto btn_resume = ui::create_filled_button("Resume",
            Rect(dt::SCREEN_W - 420, btn_y, 180, btn_h),
            on_primary);
        container->add(btn_resume);

        // End button (outlined, far right) — reuse on_primary as fallback
        auto btn_end = ui::create_outlined_button("End",
            Rect(dt::SCREEN_W - 220, btn_y, 180, btn_h),
            on_primary);
        container->add(btn_end);
    } else {
        // WARNING / CRITICAL: single acknowledge button, centered
        string btn_text = (level == ErrorLevel::CRITICAL)
            ? "Contact Support" : "Acknowledge";
        auto btn = ui::create_filled_button(btn_text,
            Rect((dt::SCREEN_W - 280) / 2, btn_y, 280, btn_h),
            on_primary);
        container->add(btn);
    }

    return container;
}
