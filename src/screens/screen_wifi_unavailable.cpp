#include "screen_wifi_unavailable.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"
#include "../ui/override_config.h"

#include <algorithm>
#include <cmath>
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

// X close glyph (two crossed Painter strokes — independent of the device
// font's ✕ glyph). Used by the popup overlay's top-right close button.
class CloseX : public Widget {
public:
    CloseX(const Rect& rect, const Color& col, function<void()> on_click)
        : Widget(rect), m_col(col), m_on_click(std::move(on_click)) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        if (m_on_click)
            on_event([this](Event&){ if (m_on_click) m_on_click(); },
                     {EventId::pointer_click});
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        const float r  = sz * 0.30f;
        painter.set(m_col);
        painter.line_width(std::max(3.0f, sz * 0.09f));
        painter.draw(Line(PointF(cx - r, cy - r), PointF(cx + r, cy + r)));
        painter.stroke();
        painter.draw(Line(PointF(cx + r, cy - r), PointF(cx - r, cy + r)));
        painter.stroke();
    }
private:
    Color m_col;
    function<void()> m_on_click;
};

// Popup body paragraph (Figma 2073:1850) — one LEFT-aligned flowing block
// with an inline green-bold "N calendar day(s)" span; the parenthetical
// paragraph continues immediately (mb-0, no blank gap) in the same white.
// EGT Labels have no rich text, so the block is custom-drawn with spans
// measured via painter.text_size at draw time (same idiom as
// screen_wifi_not_found.cpp). Line pitch 37 device px = Figma 20 px leading.
class PopupBody : public Widget {
public:
    PopupBody(const Rect& rect, int days) : Widget(rect), m_days(days) {
        fill_flags({});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
        const auto b = content_area();
        const float x0 = static_cast<float>(b.x());
        float y = static_cast<float>(b.y());
        const float lh = 37.0f;  // Figma 20 px leading x 1.852

        const Font body_font(22);
        const Font days_font(22, Font::Weight::bold);

        auto span = [&](const std::string& s, const Font& f, const Color& c,
                        float x, float yy) -> float {
            painter.set(f);
            painter.set(c);
            const auto ts = painter.text_size(s);
            painter.draw(PointF(x, yy + (lh - ts.height()) / 2.0f));
            painter.draw(s);
            return x + static_cast<float>(ts.width());
        };

        // Line 1 — white lead-in + inline green-bold day count. Only the
        // "N calendar day(s)" span is green; "from today," stays white.
        // text_size uses cairo ink extents (no trailing-space advance), so
        // the gap between the two spans is measured explicitly.
        painter.set(body_font);
        const float space_w = static_cast<float>(
            painter.text_size("o o").width() - painter.text_size("oo").width());
        float x = span("Please note that on", body_font, dt::kWhite, x0, y);
        span(to_string(m_days) + " calendar day(s)", days_font, dt::kGreen,
             x + space_w, y);
        y += lh;

        static const char* const kLines[] = {
            "from today, a Wi-Fi/Network Connection",
            "must be established, or an Override",
            "Password must be entered for device to",
            "continue to operate.",
            "(An Override Password is provided by",
            "Larada Sciences, please contact your",
            "Clinic Success contact for more details).",
        };
        for (const char* line : kLines) {
            span(line, body_font, dt::kWhite, x0, y);
            y += lh;
        }
    }
private:
    int m_days;
};

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
//      kGreen, body black (Figma 2073:1810 is text-black; supersedes the
//      ticket's original kTextPrimary wording per the parity pass).
shared_ptr<Widget> create_wifi_unavailable_screen(
    function<void()> on_continue,
    function<void()> on_back,
    function<void()> on_retry_wifi,
    function<void()> on_settings,
    bool initially_show_popup)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    const int days = ui::get_override_days();

    // ── 1) Orange banner (Figma Union 2073:1785, 432x66 @ top of frame) ────
    // Spans the full screen width. Height 66 figma px -> 122 device px.
    const int banner_h = 122;
    auto banner = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    // Figma Union band samples flat #FF9E1B; dt::kOrange (palette::kWarning,
    // #FFA500) is shared app-wide, so override locally instead of editing it
    // (same as screen_wifi_override_intro.cpp). Matches the icon PNG art.
    banner->color(Palette::ColorId::bg, Color(255, 158, 27));
    banner->border(0);
    container->add(banner);

    // Wifi-off icon (Figma Group 289, 36x36 @ frame-local (23,15) -> (43,28))
    // PNG 72x72 -> natural device 66x66, centred on the figma bbox.
    auto banner_icon = make_image_button(
        ui::asset_path("wifi-banner-icon"),
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
    //     wide, drawn as a 4 px Frame at (178,199) just below the "6".
    //
    // Group 3 is itself 255 figma px wide at frame-local x=96 -> device 472.
    // Centring the row horizontally: (800 - 472) / 2 = 164 left margin, so
    // device x starts at ~164 instead of 178. Pick 164 to feel visually
    // centred on the screen rather than left-aligned to the figma anchor.
    const int countdown_x  = 195;
    const int countdown_y  = 142;

    auto count_num = make_shared<Label>(to_string(days),
        Rect(countdown_x, countdown_y, 45, 60), AlignFlag::center);
    count_num->font(Font("Gothic A1", 44, Font::Weight::bold));
    count_num->color(Palette::ColorId::label_text, dt::kGreen);
    container->add(count_num);

    // Underline strip just under the "6" — Figma Line 2073:1814: device
    // x 178..245 (w 67), ~4 px stroke, y 199 (starts ~17 px left of glyph).
    auto count_line = make_shared<Frame>(
        Rect(178, 199, 67, 4));
    count_line->fill_flags({Theme::FillFlag::blend});
    count_line->color(Palette::ColorId::bg, dt::kGreen);
    count_line->border(0);
    container->add(count_line);

    // Figma 2073:1812 box: device left edge x=270, w 380 (205x20 figma px).
    auto count_label = make_shared<Label>("additional calendar day(s)",
        Rect(270, countdown_y + 6, 380, 44),
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
    // Figma 2073:1810 is text-black (ticket M4-19 said kTextPrimary; the
    // Figma node wins per the parity pass).
    body->color(Palette::ColorId::label_text, dt::kBlack);
    container->add(body);

    // ── 4) Continue button (Figma 2073:1807 "Group 4") ─────────────────────
    // Cyan-to-blue vertical gradient, same family as HOME's Start button.
    // Figma bbox 231x48.46 @ frame-local (100,155) -> device (185,287,428,89).
    // PNG 478x104 -> natural device 443x96. Top-left such that the button
    // portion of the PNG centres on the figma button centre.
    auto btn_continue = make_image_button(
        ui::asset_path("wifi-continue-btn"),
        Rect(178, 284, 443, 96),
        on_continue);
    container->add(btn_continue);

    // Orange info "!" icon (Figma Group 125 = 2073:1802, 21.5x21.5 @
    // frame-local (342,168.8) -> device (633,313). PNG 44x44 -> natural 40x40.
    // Wired to OPEN the explanatory popup overlay (the (!) is the affordance
    // for the modal that explains the override countdown — see popup below).
    // Forward-declared shared_ptr so the click handler can refer to it.
    auto popup_ref = make_shared<shared_ptr<Frame>>();
    auto info_icon = make_image_button(
        ui::asset_path("wifi-info-icon"),
        Rect(633, 313, 40, 40),
        [popup_ref]() { if (*popup_ref) (*popup_ref)->show(); });
    container->add(info_icon);

    // ── 5) Bottom button row: Back, Retry WiFi, Setting ────────────────────
    // Figma buttons are 116x35.3 (Back, Retry) and 99.8x35.3 (Setting) at
    // frame-local y=219 -> device y=406. PNGs are 260x98 (Back, Retry) and
    // 227x98 (Setting), naturals ~241x91 and ~210x91 respectively. Each is
    // centred on its figma bbox centre so the visible button portion lands
    // at the right place.
    auto btn_back = make_image_button(
        ui::asset_path("wifi-back-btn"),
        Rect(7, 393, 241, 91),
        on_back);
    container->add(btn_back);

    auto btn_retry = make_image_button(
        ui::asset_path("wifi-retry-btn"),
        Rect(293, 393, 241, 91),
        on_retry_wifi);
    container->add(btn_retry);

    auto btn_setting = make_image_button(
        ui::asset_path("wifi-setting-btn"),
        Rect(579, 393, 210, 91),
        on_settings);
    container->add(btn_setting);

    // ── Explanatory popup overlay (Figma 2079:2300 modal layer) ─────────────
    // Dark semi-transparent rounded card that floats over the whole screen.
    // Underlying card elements (Continue, bottom row, banner text) get dimmed
    // through the overlay — same as Figma. Shown/hidden via the (!) badge
    // above (popup_ref captures the pointer so the click handler can flip it).
    const int popup_x = 30;
    const int popup_y = 31;
    const int popup_w = 740;
    const int popup_h = 413;
    auto popup = make_shared<Frame>(Rect(popup_x, popup_y, popup_w, popup_h));
    popup->fill_flags({Theme::FillFlag::blend});
    popup->color(Palette::ColorId::bg, Color(100, 101, 105, 230));
    popup->border(0);
    popup->border_radius(15);
    *popup_ref = popup;

    // Orange (!) icon inside popup (Figma 2073:1852, top-left of overlay)
    auto popup_info = make_image_button(
        ui::asset_path("wifi-info-icon"),
        Rect(18, 13, 44, 44),
        nullptr);
    popup->add(popup_info);

    // X close — top-right of popup; hides the overlay back to the card view.
    popup->add(make_shared<CloseX>(
        Rect(popup_w - 64, 14, 44, 44), dt::kWhite,
        [popup]() { popup->hide(); }));

    // Body — Figma 2073:1850: a single LEFT-aligned flowing block at
    // popup-rel (51,34) figma -> (94,63) device, 310 figma -> 574 device
    // wide, 20 px leading -> 37 device line pitch, both paragraphs flowing
    // continuously. Custom-drawn (PopupBody) so the "N calendar day(s)"
    // span can be green-bold inline while the rest stays white.
    popup->add(make_shared<PopupBody>(Rect(94, 63, 574, 300), days));

    if (!initially_show_popup) popup->hide();
    container->add(popup);

    return container;
}
