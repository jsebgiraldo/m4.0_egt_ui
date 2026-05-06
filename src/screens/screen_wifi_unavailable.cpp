#include "screen_wifi_unavailable.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_wifi_unavailable_screen(
    function<void()> on_continue)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo (centered, upper)
    const int logo_w = 200, logo_h = 125;
    auto logo = ui::create_logo(
        (dt::SCREEN_W - logo_w) / 2, 30, logo_w, logo_h);
    container->add(logo);

    // "Setup" label (top area, Figma style)
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
        "Wi-Fi Network is\nTemporarily Unavailable",
        Rect(50, 230, dt::SCREEN_W - 100, 80));
    msg->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    msg->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(msg);

    // Sub-message
    auto sub_msg = make_shared<Label>(
        "The device cannot connect to a Wi-Fi network.\n"
        "You may continue in Override Mode.",
        Rect(50, 320, dt::SCREEN_W - 100, 60));
    sub_msg->font(Font(dt::FONT_BODY, Font::Weight::normal));
    sub_msg->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(sub_msg);

    // "Continue" button (centered, filled)
    auto btn = ui::create_filled_button("Continue",
        Rect((dt::SCREEN_W - 240) / 2, dt::SCREEN_H - 90, 240, 70),
        on_continue);
    container->add(btn);

    return container;
}
