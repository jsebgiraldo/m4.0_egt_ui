// absolute-positioning.cpp
//
// How this project converts a Figma node (canvas 432x261) to an EGT widget
// at absolute pixel coordinates on the 800x480 panel. Scale factor is
// dt::SCALE = 800.0 / 432.0 = 1.852.
//
// Pulled from src/screens/screen_wifi_init.cpp and screen_home.cpp which
// render correctly on hardware.

#include <egt/ui>
#include "ui/design_tokens.h"   // dt::SCREEN_W, dt::SCREEN_H, dt::SCALE

using namespace egt;

std::shared_ptr<Widget> example_screen()
{
    // 1. Screen-sized container. Children's positions are LOCAL to this frame.
    auto container = std::make_shared<Frame>(
        Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, Color(0xFFFFFFFF));

    // 2. Each Figma node -> Rect(x * SCALE, y * SCALE, w * SCALE, h * SCALE).
    // The numbers below are already scaled; we keep both forms in comments.
    //
    // Figma 90x56 at (4,9) -> 167x104 at (7,17).
    auto logo = std::make_shared<ImageLabel>(
        Image("file:assets/figma/images/logo.png"));
    logo->box(Rect(7, 17, 167, 104));
    logo->image_align(AlignFlag::center | AlignFlag::expand);
    logo->autoresize(false);   // belt-and-suspenders, see egt-widget-reference.md
    container->add(logo);

    // 3. A label placed at absolute coords inside the container.
    // Coords here are local to `container`. Since container's box origin
    // is (0,0), local == display in this case. If container had been at
    // (50, 0), then writing Rect(7, 17, ...) here would still mean
    // "(7, 17) inside container" -> draw at (57, 17) on the screen.
    auto title = std::make_shared<Label>("Wi-Fi Connected", Rect(319, 178, 300, 46));
    title->font(Font(37, Font::Weight::bold));
    title->text_align(AlignFlag::left);
    container->add(title);

    // 4. Button via constructor Rect == move() + resize() combined.
    auto btn = std::make_shared<Button>("Continue", Rect(302, 355, 204, 61));
    btn->border(2);
    container->add(btn);

    return container;
}
