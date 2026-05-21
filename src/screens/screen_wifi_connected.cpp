#include "screen_wifi_connected.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

using namespace egt;
using namespace std;

shared_ptr<Widget> create_wifi_connected_screen(
    function<void()> on_continue)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Figma v5 Group 266: every dimension below = Figma raw * dt::SCALE (1.852).

    // Logo top-left: Figma 90x56 at (4,9) -> 167x104 at (7,17).
    auto logo = ui::create_logo(7, 17, 167, 104);
    container->add(logo);

    // Check circle: Figma 35x35 at (127,86) -> 65x65 at (235,159). Exported
    // from Figma node 2065:1047 (Group 175 = ellipse + check vector) as a
    // 4x PNG; loaded pre-scaled so the natural size matches the target rect.
    constexpr int check_sz = 65;
    try {
        const float check_scale = static_cast<float>(check_sz) / 148.0f;  // PNG 148x148
        auto check_img = Image("file:assets/figma/images/check-circle-green.png",
                               check_scale, check_scale);
        auto check = make_shared<ImageLabel>(check_img);
        check->autoresize(false);
        check->fill_flags({Theme::FillFlag::blend});
        check->image_align(AlignFlag::center);
        check->box(Rect(235, 159, check_sz, check_sz));
        container->add(check);
    } catch (const std::exception& e) {
        printf("[WIFI_CONNECTED] check icon asset missing: %s\n", e.what());
        fflush(stdout);
    }

    // Title "Wi-Fi Connected": Figma 162x25 at (172,96) -> 300x46 at (319,178),
    // Gothic A1 Bold 20pt -> 37pt scaled.
    auto title = make_shared<Label>("Wi-Fi Connected",
        Rect(319, 178, 300, 46));
    title->font(Font(37, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kGreen);
    title->text_align(AlignFlag::left);
    container->add(title);

    // Subtitle (two lines): Figma 267x36 at (90,140) -> 494x67 at (167,259),
    // Gothic A1 Regular 14pt -> 26pt scaled.
    auto sub = make_shared<Label>(
        "WiFi connection established successfully.\nThe device is ready to use.",
        Rect(167, 259, 494, 67));
    sub->font(Font(26, Font::Weight::normal));
    sub->color(Palette::ColorId::label_text, dt::kTextPrimary);
    sub->text_align(AlignFlag::center);
    container->add(sub);

    // Continue button: Figma 110x33 at (163,192) -> 204x61 at (302,355).
    // The button + chevron live inside a wrapper Frame so they share the
    // same local coordinate system. This sidesteps the ~30 px y-offset that
    // appears when an ImageLabel is positioned directly inside the screen
    // container alongside a Button at the same absolute coords.
    constexpr int chev_w = 22;
    constexpr int chev_h = 39;
    const Rect btn_rect(302, 355, 204, 61);

    auto btn_wrap = make_shared<Frame>(btn_rect);
    btn_wrap->fill_flags({});                                   // transparent
    container->add(btn_wrap);

    // Build the button inline rather than using ui::create_outlined_button:
    // the helper calls btn->font() before we can disable autoresize, which
    // grows the box past 204x61. Doing it here lets us flip the autoresize
    // flag immediately after construction.
    auto btn = make_shared<Button>("Continue",
        Rect(0, 0, btn_rect.width(), btn_rect.height()));
    btn->autoresize(false);                              // FIRST, locks size
    btn->font(Font(26, Font::Weight::bold));
    btn->text_align(AlignFlag::left | AlignFlag::center_vertical);
    btn->color(Palette::ColorId::button_bg, dt::kWhite);
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->color(Palette::ColorId::border, dt::kGrayLight);
    btn->border(2);
    btn->border_radius(dt::RADIUS_MD);
    if (on_continue) {
        auto cb = std::move(on_continue);
        btn->on_click([cb](Event&) { cb(); });
    }
    btn_wrap->add(btn);

    // ImageHolder::do_set_image() auto-resizes the widget to the image's
    // natural pixel size and Widget::autoresize defaults to true, which can
    // re-grow the box during a later layout() pass. Two-step protection:
    //   1. Pre-scale the Image at load time via Image(uri, hscale, vscale)
    //      so its natural size already matches the target rect.
    //   2. Set autoresize(false) so any future layout pass cannot grow it.
    // See docs/08-references/egt-widget-reference.md ("ImageLabel" section).
    try {
        const float hscale = static_cast<float>(chev_w) / 35.0f;  // PNG src 35x52
        const float vscale = static_cast<float>(chev_h) / 52.0f;
        auto chev_img = Image("file:assets/figma/images/chevron-right.png", hscale, vscale);
        auto chevron = make_shared<ImageLabel>(chev_img);
        chevron->autoresize(false);
        chevron->fill_flags({Theme::FillFlag::blend});
        chevron->image_align(AlignFlag::center);  // no `expand`
        // Wrap-local coords (parent Frame is at btn_rect, so 0,0 is its origin).
        chevron->box(Rect(btn_rect.width() - chev_w - 22,
                          (btn_rect.height() - chev_h) / 2,
                          chev_w, chev_h));
        btn_wrap->add(chevron);
    } catch (const std::exception& e) {
        printf("[CONTINUE] chevron asset missing: %s\n", e.what());
        fflush(stdout);
    }

    return container;
}
