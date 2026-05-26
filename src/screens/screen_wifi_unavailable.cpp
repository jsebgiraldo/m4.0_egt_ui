#include "screen_wifi_unavailable.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <algorithm>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

namespace {

// Same pattern as screen_demo_info.cpp - a PNG exported by Figma at scale=2,
// placed in a Frame sized to the PNG's natural device size (PNG_px * SCALE/2).
// The Frame captures clicks and the ImageLabel renders the art.
shared_ptr<Frame> make_image_button(
    const string& png_path,
    const Rect& rect,
    function<void()> on_click)
{
    auto wrap = make_shared<Frame>(rect);
    wrap->fill_flags({});

    try {
        auto probe = Image(("file:" + png_path).c_str());
        const float sw = static_cast<float>(probe.width());
        const float sh = static_cast<float>(probe.height());
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
        printf("[WIFI_UNAVAIL] image %s missing: %s\n", png_path.c_str(), e.what());
        fflush(stdout);
    }

    wrap->on_event([on_click](Event& e) {
        if (e.id() == EventId::pointer_click && on_click) on_click();
    });

    return wrap;
}

} // namespace

// ── WIFI_UNAVAILABLE screen (Figma node 2079:2300, "Group 308" - M4-19) ─────
// Figma frame: 432x262 at (-1117, 1992) -> 800x480 device-px (SCALE = 1.852).
//
// Layout (top to bottom):
//   - Orange banner: full-width, wifi-off icon + "remains unavailable" text
//   - Countdown row: big green number with underline + "additional calendar
//     day(s)" in green bold
//   - Body line: "It will still save all treatment information ..."
//   - Cyan gradient Continue button (opens override flow) + orange info icon
//   - Three bottom buttons: Back, Retry WiFi, Setting
//
// Acceptance criteria from ticket M4-19:
//   1. Card fills the full screen (no outer padding) - container is now the
//      full 800x480 with a flat layout.
//   2. Info icon enlarged (was non-existent, now a 40x40 PNG from Figma).
//   3. Underline below the countdown number - drawn as a 2 px green Frame
//      under the "6" label.
//   4. Button icons and text match Figma - all three bottom buttons are
//      now PNG exports of the Figma button COMPONENTs.
//   5. Text centring and colour - banner text centred, countdown text in
//      kGreen, body in kTextPrimary.
shared_ptr<Widget> create_wifi_unavailable_screen(
    function<void()> on_retry_wifi,
    function<void()> on_settings,
    function<void()> on_override,
    function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── 1) Orange banner (Figma Union 2073:1785, 432x66 @ top of frame) ────
    // Spans the full screen width. Height 66 figma px -> 122 device px.
    const int banner_h = 122;
    auto banner = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, dt::kOrange);
    banner->border(0);
    container->add(banner);

    // Wifi-off icon (Figma Group 289, 36x36 @ frame-local (23,15) -> (43,28))
    // PNG 72x72 -> natural device 66x66, centred on the figma bbox.
    auto banner_icon = make_image_button(
        "assets/figma/images/wifi-banner-icon.png",
        Rect(43, 28, 67, 67),
        nullptr);
    container->add(banner_icon);

    // Banner text (Figma 2073:1800, 280x30 @ frame-local (85,18) -> (157,33))
    // fontSize 12 Medium (500) -> 22 pt. Two lines, centred horizontally,
    // black on orange.
    auto banner_text = make_shared<Label>(
        "Wi-Fi/Network Connection remains unavailable,\n"
        "device will continue to operate normally for",
        Rect(120, 24, dt::SCREEN_W - 240, 74), AlignFlag::center);
    banner_text->font(Font("Gothic A1", 22, Font::Weight::normal));
    banner_text->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(banner_text);

    // ── 2) Countdown line (Group 3 = 2073:1811) ────────────────────────────
    // Three sub-elements:
    //   - big "6" (2073:1813): fontSize 24 Bold green @ (10,80) -> (19,148),
    //     size 21x30 -> 39x56 device.
    //   - "additional calendar day(s)" (2073:1812): fontSize 16 Bold green @
    //     (50,83) -> (93,154), size 205x20 -> 380x37 device.
    //   - underline LINE (2073:1814): 36x0 @ (0,110) -> (0,204), 67 device px
    //     wide, drawn as a 2 px Frame at y=204 just below the "6".
    //
    // Group 3 is itself 255 figma px wide at frame-local x=96 -> device 472.
    // Centring the row horizontally: (800 - 472) / 2 = 164 left margin, so
    // device x starts at ~164 instead of 178. Pick 164 to feel visually
    // centred on the screen rather than left-aligned to the figma anchor.
    const int countdown_x  = 195;
    const int countdown_y  = 142;
    const int countdown_w  = 415;  // covers "6" + label + ample air

    auto count_num = make_shared<Label>("6",
        Rect(countdown_x, countdown_y, 45, 60), AlignFlag::center);
    count_num->font(Font("Gothic A1", 44, Font::Weight::bold));
    count_num->color(Palette::ColorId::label_text, dt::kGreen);
    container->add(count_num);

    // Underline strip just under the "6"
    auto count_line = make_shared<Frame>(
        Rect(countdown_x - 2, countdown_y + 56, 49, 3));
    count_line->fill_flags({Theme::FillFlag::blend});
    count_line->color(Palette::ColorId::bg, dt::kGreen);
    count_line->border(0);
    container->add(count_line);

    auto count_label = make_shared<Label>("additional calendar day(s)",
        Rect(countdown_x + 50, countdown_y + 6, countdown_w - 50, 44),
        AlignFlag::left | AlignFlag::center_vertical);
    count_label->font(Font("Gothic A1", 30, Font::Weight::bold));
    count_label->color(Palette::ColorId::label_text, dt::kGreen);
    container->add(count_label);

    // ── 3) Body text (Figma 2073:1810, 348x30 @ (41,117) -> (76,217)) ──────
    // fontSize 12 Regular black -> 22 pt. Two lines centred.
    auto body = make_shared<Label>(
        "It will still save all treatment information on device\n"
        "& transmit once Connection is established.",
        Rect(0, 215, dt::SCREEN_W, 60), AlignFlag::center);
    body->font(Font("Gothic A1", 22, Font::Weight::normal));
    body->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(body);

    // ── 4) Continue button (Figma 2073:1807 "Group 4") ─────────────────────
    // Cyan-to-blue vertical gradient, same family as HOME's Start button.
    // Figma bbox 231x48.46 @ frame-local (100,155) -> device (185,287,428,89).
    // PNG 478x104 -> natural device 443x96. Top-left such that the button
    // portion of the PNG centres on the figma button centre.
    auto btn_continue = make_image_button(
        "assets/figma/images/wifi-continue-btn.png",
        Rect(178, 284, 443, 96),
        on_override);
    container->add(btn_continue);

    // Orange info "!" icon (Figma Group 125 = 2073:1802, 21.5x21.5 @
    // frame-local (342,168.8) -> device (633,313). PNG 44x44 -> natural 40x40.
    // Wired to on_override too so users have a second affordance for the
    // override flow (matches the "info-explains-the-override" relationship
    // shown in the Figma modal overlay).
    auto info_icon = make_image_button(
        "assets/figma/images/wifi-info-icon.png",
        Rect(633, 313, 40, 40),
        on_override);
    container->add(info_icon);

    // ── 5) Bottom button row: Back, Retry WiFi, Setting ────────────────────
    // Figma buttons are 116x35.3 (Back, Retry) and 99.8x35.3 (Setting) at
    // frame-local y=219 -> device y=406. PNGs are 260x98 (Back, Retry) and
    // 227x98 (Setting), naturals ~241x91 and ~210x91 respectively. Each is
    // centred on its figma bbox centre so the visible button portion lands
    // at the right place.
    auto btn_back = make_image_button(
        "assets/figma/images/wifi-back-btn.png",
        Rect(7, 393, 241, 91),
        on_back);
    container->add(btn_back);

    auto btn_retry = make_image_button(
        "assets/figma/images/wifi-retry-btn.png",
        Rect(293, 393, 241, 91),
        on_retry_wifi);
    container->add(btn_retry);

    auto btn_setting = make_image_button(
        "assets/figma/images/wifi-setting-btn.png",
        Rect(579, 393, 210, 91),
        on_settings);
    container->add(btn_setting);

    return container;
}
