#include "screen_wifi_override_info.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_wifi_override_info_screen(
    function<void()> on_continue,
    function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo (centered, upper)
    const int logo_w = 200, logo_h = 125;
    auto logo = ui::create_logo(
        (dt::SCREEN_W - logo_w) / 2, 30, logo_w, logo_h);
    container->add(logo);

    // "Setup" label
    auto setup_lbl = make_shared<Label>("Setup",
        Rect(0, 170, dt::SCREEN_W, 30));
    setup_lbl->font(Font(dt::FONT_SUBTITLE, Font::Weight::bold));
    setup_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(setup_lbl);

    // Divider
    auto divider = make_shared<Frame>(Rect(50, 210, dt::SCREEN_W - 100, 2));
    divider->fill_flags({Theme::FillFlag::blend});
    divider->color(Palette::ColorId::bg, dt::kGrayLight);
    divider->border(0);
    container->add(divider);

    // Main message
    auto msg = make_shared<Label>(
        "Wi-Fi / Network Connection\nremains unavailable",
        Rect(50, 230, dt::SCREEN_W - 100, 60));
    msg->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    msg->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(msg);

    // Info text
    auto info = make_shared<Label>(
        "To continue without Wi-Fi, enter the\n"
        "Override password on the next screen.",
        Rect(50, 305, dt::SCREEN_W - 100, 60));
    info->font(Font(dt::FONT_BODY, Font::Weight::normal));
    info->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(info);

    // Back button (bottom-left)
    auto btn_back = ui::create_outlined_button("Back",
        Rect(40, dt::SCREEN_H - 90, 180, 70),
        on_back);
    container->add(btn_back);

    // Continue button (bottom-right)
    auto btn_continue = ui::create_filled_button("Continue",
        Rect(dt::SCREEN_W - 220, dt::SCREEN_H - 90, 180, 70),
        on_continue);
    container->add(btn_continue);

    return container;
}
