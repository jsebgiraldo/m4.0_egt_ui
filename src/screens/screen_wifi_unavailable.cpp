#include "screen_wifi_unavailable.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <egt/svgimage.h>

#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

using namespace egt;
using namespace std;

// ── Shared SVG glyphs (same gear as HOME, plus matching refresh arrow) ─────
// Using the same SVG that screen_home.cpp uses keeps the icon language
// consistent across the device — "the same gear means the same thing".
static const char* kGearSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M19.44 12.99c.04-.33.07-.66.07-1s-.03-.67-.07-1l2.44-1.92-2.32-4-2.82 1.17c-.5-.37-1.04-.69-1.63-.94l-.37-3h-4.64l-.38 3c-.59.25-1.12.57-1.62.94l-2.82-1.17-2.32 4 2.44 1.92c-.04.33-.07.66-.07 1s.03.67.07 1l-2.44 1.92 2.32 4 2.82-1.17c.5.37 1.04.69 1.63.94l.38 3h4.64l.38-3c.59-.25 1.12-.57 1.62-.94l2.82 1.17 2.32-4-2.44-1.92zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z" fill="#646469"/>
</svg>)svg";

// Refresh / reload glyph — a 3/4 circle with a small arrowhead.
static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.74 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646469"/>
</svg>)svg";

static string write_svg_tmp(const char* name, const char* svg_data)
{
    string path = string("/tmp/egt-icon-") + name + ".svg";
    static bool gear_written    = false;
    static bool refresh_written = false;
    bool& written = (string(name) == "gear") ? gear_written : refresh_written;
    if (!written) {
        ofstream f(path);
        f << svg_data;
        written = f.good();
    }
    return path;
}

static Image load_svg_icon(const char* name, const char* svg_data, int size)
{
    try {
        auto path = write_svg_tmp(name, svg_data);
        SvgImage svg("file:" + path, SizeF(size, size));
        return static_cast<Image>(svg);
    } catch (...) {
        return {};
    }
}

// ── Glyphs for the screen ──────────────────────────────────────────────────
namespace {

// Wi-Fi-with-slash glyph — used in the orange banner and inside the override
// card. Three concentric arcs (open downward) + a base dot + a diagonal
// slash through the whole thing. The colour is configurable so we can paint
// it white on the orange banner and dark on the white card.
class WifiOffGlyph : public Widget {
public:
    WifiOffGlyph(const Rect& rect, const Color& col)
        : Widget(rect), m_col(col)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void set_color(const Color& col) { m_col = col; damage(); }

    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;

        constexpr float start = -static_cast<float>(M_PI) * 0.75f;
        constexpr float end   = -static_cast<float>(M_PI) * 0.25f;
        const auto pivot = PointF(cx, cy + sz * 0.18f);
        const float radii[] = {sz * 0.34f, sz * 0.24f, sz * 0.13f};

        painter.set(m_col);
        painter.line_width(std::max(2.0f, sz * 0.055f));
        for (float r : radii) {
            painter.draw(Arc(pivot, r, start, end));
            painter.stroke();
        }
        painter.draw(Arc(pivot, sz * 0.05f, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();

        painter.line_width(std::max(2.5f, sz * 0.065f));
        painter.draw(Line(PointF(cx - sz * 0.32f, cy - sz * 0.23f),
                          PointF(cx + sz * 0.32f, cy + sz * 0.23f)));
        painter.stroke();
    }

private:
    Color m_col;
};

// ── Building blocks ────────────────────────────────────────────────────────

// Bottom strip "icon button" — gray circle background + glyph + label.
// Used for Retry WiFi and Setting; the glyph is an Image (the SvgImage
// rendered to a bitmap at the right size) so we get the same crisp gear/
// refresh art as the rest of the app.
shared_ptr<Frame> make_icon_button(int x, int y, int w, int h,
                                   const string& label,
                                   const Image& icon,
                                   function<void()> on_click)
{
    auto frame = make_shared<Frame>(Rect(x, y, w, h));
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, dt::kBgWhite);
    frame->border(1);
    frame->color(Palette::ColorId::border, dt::kGrayLight);
    frame->border_radius(8);

    const int circle_d = 36;
    const int circle_x = 14;
    const int circle_y = (h - circle_d) / 2;
    auto bg = make_shared<Frame>(Rect(circle_x, circle_y, circle_d, circle_d));
    bg->fill_flags({Theme::FillFlag::blend});
    bg->color(Palette::ColorId::bg, palette::kGray200);
    bg->border(0);
    bg->border_radius(circle_d / 2);
    frame->add(bg);

    if (!icon.empty()) {
        const int icon_sz = 22;
        auto icon_lbl = make_shared<ImageLabel>(icon);
        icon_lbl->fill_flags({});
        icon_lbl->color(Palette::ColorId::bg, palette::kGray200);
        icon_lbl->image_align(AlignFlag::center);
        icon_lbl->move(Point(circle_x + (circle_d - icon_sz) / 2,
                             circle_y + (circle_d - icon_sz) / 2));
        icon_lbl->resize(Size(icon_sz, icon_sz));
        frame->add(icon_lbl);
    }

    auto lbl = make_shared<Label>(label,
        Rect(circle_x + circle_d + 12, 0,
             w - (circle_x + circle_d + 12) - 8, h));
    lbl->font(Font(15, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    frame->add(lbl);

    frame->on_event([on_click](Event&) { if (on_click) on_click(); },
                    {EventId::pointer_click});

    return frame;
}

} // namespace

// ── Screen entry point ─────────────────────────────────────────────────────
shared_ptr<Widget> create_wifi_unavailable_screen(
    function<void()> on_retry_wifi,
    function<void()> on_settings,
    function<void()> on_override,
    function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Outer card hosts the banner (top stripe) + override card + 2 button
    // chips. The card itself has no header label — the Figma frame 52:2774
    // is just the orange banner + content rows.
    const int card_x = 40;
    const int card_y = 40;
    const int card_w = dt::SCREEN_W - 80;
    const int card_h = 400;

    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kBgWhite);
    card->border(1);
    card->color(Palette::ColorId::border, dt::kGrayLight);
    card->border_radius(12);
    container->add(card);

    // ── 1) Orange banner ───────────────────────────────────────────────────
    // Per Figma 52:2774: banner spans the full width, text is centred and
    // dark (not white-left). The wifi-off chip sits floating top-left and
    // does NOT push the text — the text rect uses the full banner width so
    // the two lines read centred on the page axis.
    const int banner_h = 90;
    auto banner = make_shared<Frame>(Rect(0, 0, card_w, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, dt::kOrange);
    banner->border(0);
    banner->border_radius(12);
    card->add(banner);

    // Wi-Fi-off glyph inside a white round chip — floats at the top-left.
    const int chip_d = 56;
    const int chip_x = 20;
    const int chip_y = (banner_h - chip_d) / 2;
    auto chip = make_shared<Frame>(Rect(chip_x, chip_y, chip_d, chip_d));
    chip->fill_flags({Theme::FillFlag::blend});
    chip->color(Palette::ColorId::bg, dt::kWhite);
    chip->border(0);
    chip->border_radius(chip_d / 2);
    banner->add(chip);
    auto chip_glyph = make_shared<WifiOffGlyph>(
        Rect(chip_x, chip_y, chip_d, chip_d), dt::kTextPrimary);
    banner->add(chip_glyph);

    // Two centred dark lines spanning the full banner width.
    auto banner_l1 = make_shared<Label>("Wi-Fi Network not found.",
        Rect(0, 18, card_w, 26));
    banner_l1->font(Font(18, Font::Weight::bold));
    banner_l1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    banner_l1->text_align(AlignFlag::center);
    banner->add(banner_l1);

    auto banner_l2 = make_shared<Label>("No available network detected.",
        Rect(0, 46, card_w, 26));
    banner_l2->font(Font(16, Font::Weight::normal));
    banner_l2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    banner_l2->text_align(AlignFlag::center);
    banner->add(banner_l2);

    // ── 2) Override card — tap to toggle "selected" (cyan) then again to confirm ─
    const int ov_y = banner_h + 20;
    const int ov_w = card_w - 60;
    const int ov_h = 110;
    const int ov_x = (card_w - ov_w) / 2;
    auto ov_card = make_shared<Frame>(Rect(ov_x, ov_y, ov_w, ov_h));
    ov_card->fill_flags({Theme::FillFlag::blend});
    ov_card->color(Palette::ColorId::bg, dt::kBgWhite);
    ov_card->border(1);
    ov_card->color(Palette::ColorId::border, dt::kGrayLight);
    ov_card->border_radius(10);
    card->add(ov_card);

    // Override icon (wifi-off in a gray pill)
    const int ov_glyph_d = 48;
    auto ov_chip = make_shared<Frame>(
        Rect(20, (ov_h - ov_glyph_d) / 2, ov_glyph_d, ov_glyph_d));
    ov_chip->fill_flags({Theme::FillFlag::blend});
    ov_chip->color(Palette::ColorId::bg, palette::kGray200);
    ov_chip->border(0);
    ov_chip->border_radius(ov_glyph_d / 2);
    ov_card->add(ov_chip);
    auto ov_glyph = make_shared<WifiOffGlyph>(
        Rect(20, (ov_h - ov_glyph_d) / 2, ov_glyph_d, ov_glyph_d), dt::kTextPrimary);
    ov_card->add(ov_glyph);

    auto ov_line1 = make_shared<Label>("Operate without WiFi",
        Rect(20 + ov_glyph_d + 16, 22, ov_w - (20 + ov_glyph_d + 16) - 20, 24));
    ov_line1->font(Font(16, Font::Weight::bold));
    ov_line1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    ov_line1->text_align(AlignFlag::left | AlignFlag::center_vertical);
    ov_card->add(ov_line1);

    auto ov_line2 = make_shared<Label>("Temporarily operate device in",
        Rect(20 + ov_glyph_d + 16, 50, ov_w - (20 + ov_glyph_d + 16) - 20, 22));
    ov_line2->font(Font(13, Font::Weight::normal));
    ov_line2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    ov_line2->text_align(AlignFlag::left | AlignFlag::center_vertical);
    ov_card->add(ov_line2);

    auto ov_line3 = make_shared<Label>("OVERRIDE MODE.",
        Rect(20 + ov_glyph_d + 16, 70, ov_w - (20 + ov_glyph_d + 16) - 20, 22));
    ov_line3->font(Font(13, Font::Weight::bold));
    ov_line3->color(Palette::ColorId::label_text, dt::kTextPrimary);
    ov_line3->text_align(AlignFlag::left | AlignFlag::center_vertical);
    ov_card->add(ov_line3);

    // Two-step interaction: first tap "selects" the card (blue), second tap
    // commits and triggers the override flow. Matches Figma State-1 → State-2
    // transition.
    auto selected = make_shared<bool>(false);
    ov_card->on_event([=](Event&) {
        if (*selected) {
            if (on_override) on_override();
        } else {
            *selected = true;
            ov_card->color(Palette::ColorId::bg, dt::kAccentCyan);
            ov_chip->color(Palette::ColorId::bg, Color(0, 0, 0, 50));
            ov_glyph->set_color(dt::kWhite);
            ov_line1->color(Palette::ColorId::label_text, dt::kWhite);
            ov_line2->color(Palette::ColorId::label_text, dt::kWhite);
            ov_line3->color(Palette::ColorId::label_text, dt::kWhite);
            ov_card->damage();
        }
    }, {EventId::pointer_click});

    // ── 3) Bottom row: Retry WiFi + Setting ───────────────────────────────
    const int row_y = ov_y + ov_h + 24;
    const int row_h = 56;
    const int row_gap = 18;
    const int row_w = (card_w - 30 * 2 - row_gap) / 2;

    // Same gear glyph as HOME / Settings — single source of icon truth.
    auto refresh_icon = load_svg_icon("refresh", kRefreshSvg, 22);
    auto gear_icon    = load_svg_icon("gear",    kGearSvg,    22);

    auto retry = make_icon_button(
        30, row_y, row_w, row_h, "Retry WiFi", refresh_icon, on_retry_wifi);
    card->add(retry);

    auto setting = make_icon_button(
        30 + row_w + row_gap, row_y, row_w, row_h, "Setting", gear_icon, on_settings);
    card->add(setting);

    // Per Figma 52:2774 — no Back button on this screen. Navigation away
    // happens through Retry WiFi / Setting / Override only.
    (void)on_back;

    return container;
}
