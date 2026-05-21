// icon-over-button.cpp
//
// The canonical pattern this project uses to place a chevron / icon image
// at a precise offset inside an outlined Button. Two siblings inside a
// dedicated wrapper Frame, so both share the same local coordinate space.
//
// Working example from src/screens/screen_wifi_connected.cpp.
//
// Why a wrapper Frame: when the icon (an ImageLabel) and the Button were
// siblings of the full-screen container, the ImageLabel re-positioned at
// layout() time because of its auto-resize and the size of the source PNG.
// Putting both inside a small wrapper Frame at the button's rect means
// (a) child coords are local to the wrapper, (b) the layout pass on the
// wrapper has no other children to fight with, (c) the wrapper itself is
// not resized by the children.

#include <egt/ui>
#include <cmath>

using namespace egt;

std::shared_ptr<Widget> button_with_chevron()
{
    constexpr int chev_w = 22;
    constexpr int chev_h = 39;
    const Rect btn_rect(302, 355, 204, 61);

    // 1. Wrapper frame at the button's absolute rect.
    auto wrap = std::make_shared<Frame>(btn_rect);
    wrap->fill_flags({});   // transparent

    // 2. Button fills the wrapper, so coords inside the wrapper are 0..w/h.
    auto btn = std::make_shared<Button>("Continue",
                                        Rect(0, 0, btn_rect.width(), btn_rect.height()));
    btn->border(2);
    btn->font(Font(26, Font::Weight::bold));
    wrap->add(btn);

    // 3. Chevron image. Pre-scale at load so the image's natural size
    // already matches the on-screen size we want. This avoids the
    // image_align(expand) path entirely.
    const float hscale = static_cast<float>(chev_w) / 35.0f;  // PNG src 35x52
    const float vscale = static_cast<float>(chev_h) / 52.0f;
    auto img = Image("file:assets/image/chevron-right.png", hscale, vscale);

    auto chevron = std::make_shared<ImageLabel>(img);
    chevron->fill_flags({Theme::FillFlag::blend});
    chevron->image_align(AlignFlag::center);   // centred inside its own box

    // 4. Position the chevron in wrapper-local coords. Vertically centred
    // in the button by computing (btn_h - chev_h) / 2, horizontally 22 px
    // from the right edge of the button.
    chevron->box(Rect(btn_rect.width() - chev_w - 22,
                      (btn_rect.height() - chev_h) / 2,
                      chev_w, chev_h));
    wrap->add(chevron);

    return wrap;
}
