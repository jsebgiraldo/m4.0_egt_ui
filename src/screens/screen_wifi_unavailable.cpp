#include "screen_wifi_unavailable.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <cmath>
#include <memory>
#include <utility>

using namespace egt;
using namespace std;

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

// Refresh / reload arrow — open circle with a small arrowhead.
class RefreshGlyph : public Widget {
public:
    RefreshGlyph(const Rect& rect, const Color& col)
        : Widget(rect), m_col(col)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        const float r  = sz * 0.32f;

        painter.set(m_col);
        painter.line_width(std::max(2.2f, sz * 0.06f));
        // 3/4 arc (open at top-right)
        painter.draw(Arc(PointF(cx, cy), r,
                         static_cast<float>(M_PI) * -0.35f,
                         static_cast<float>(M_PI) * 1.55f));
        painter.stroke();

        // Small arrowhead at the open end (top-right)
        const float ax = cx + r * std::cos(static_cast<float>(M_PI) * -0.35f);
        const float ay = cy + r * std::sin(static_cast<float>(M_PI) * -0.35f);
        const float head = sz * 0.13f;
        painter.draw(Line(PointF(ax, ay), PointF(ax - head, ay - head * 0.3f)));
        painter.stroke();
        painter.draw(Line(PointF(ax, ay), PointF(ax + head * 0.2f, ay + head)));
        painter.stroke();
    }
private:
    Color m_col;
};

// Gear / settings — 8-tooth wheel with central circle. Drawn with Painter so
// the icon scales cleanly inside its container without an SVG asset.
class GearGlyph : public Widget {
public:
    GearGlyph(const Rect& rect, const Color& col)
        : Widget(rect), m_col(col)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        const float r_outer = sz * 0.42f;
        const float r_inner = sz * 0.28f;
        const float r_hole  = sz * 0.13f;
        const auto  TAU     = 2.0f * static_cast<float>(M_PI);

        painter.set(m_col);
        painter.line_width(std::max(2.2f, sz * 0.055f));

        // 8 teeth — short lines extending outward
        for (int i = 0; i < 8; ++i) {
            float a = TAU * static_cast<float>(i) / 8.0f;
            painter.draw(Line(
                PointF(cx + r_inner * std::cos(a), cy + r_inner * std::sin(a)),
                PointF(cx + r_outer * std::cos(a), cy + r_outer * std::sin(a))));
            painter.stroke();
        }
        // Main gear body (ring)
        painter.draw(Arc(PointF(cx, cy), r_inner, 0.0f, TAU));
        painter.stroke();
        // Central hole
        painter.draw(Arc(PointF(cx, cy), r_hole, 0.0f, TAU));
        painter.stroke();
    }
private:
    Color m_col;
};

// Small "i in a circle" info badge — used on the banner left corner.
class InfoBadge : public Widget {
public:
    InfoBadge(const Rect& rect, const Color& fg, const Color& bg)
        : Widget(rect), m_fg(fg), m_bg(bg)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        const float r  = sz * 0.45f;

        painter.set(m_bg);
        painter.draw(Arc(PointF(cx, cy), r, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();

        // dot of the "i"
        painter.set(m_fg);
        painter.draw(Arc(PointF(cx, cy - sz * 0.20f), sz * 0.06f,
                         0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
        // stem of the "i"
        painter.line_width(std::max(2.5f, sz * 0.10f));
        painter.draw(Line(PointF(cx, cy - sz * 0.05f),
                          PointF(cx, cy + sz * 0.20f)));
        painter.stroke();
    }
private:
    Color m_fg, m_bg;
};

// ── Building blocks ────────────────────────────────────────────────────────

// Bottom strip "icon button" — circular icon + label below or to the right.
// Used for Retry WiFi and Setting.
struct IconButton {
    shared_ptr<Frame> frame;
    shared_ptr<Frame> bg_circle;
};

IconButton make_icon_button(int x, int y, int w, int h,
                            const string& label,
                            function<shared_ptr<Widget>(const Rect&)> make_glyph,
                            function<void()> on_click)
{
    auto frame = make_shared<Frame>(Rect(x, y, w, h));
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, dt::kBgWhite);
    frame->border(1);
    frame->color(Palette::ColorId::border, dt::kGrayLight);
    frame->border_radius(8);

    const int glyph_sz = 30;
    auto bg = make_shared<Frame>(Rect(12, (h - glyph_sz) / 2, glyph_sz, glyph_sz));
    bg->fill_flags({Theme::FillFlag::blend});
    bg->color(Palette::ColorId::bg, palette::kGray200);
    bg->border(0);
    bg->border_radius(glyph_sz / 2);
    frame->add(bg);

    auto glyph = make_glyph(Rect(12, (h - glyph_sz) / 2, glyph_sz, glyph_sz));
    frame->add(glyph);

    auto lbl = make_shared<Label>(label,
        Rect(12 + glyph_sz + 8, 0, w - (12 + glyph_sz + 8) - 8, h));
    lbl->font(Font(14, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    frame->add(lbl);

    frame->on_event([on_click](Event&) { if (on_click) on_click(); },
                    {EventId::pointer_click});

    return {frame, bg};
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

    // Page-level title
    auto title = make_shared<Label>("Not Connected",
        Rect(20, 12, dt::SCREEN_W - 40, 28));
    title->font(Font(18, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    title->text_align(AlignFlag::left | AlignFlag::center_vertical);
    container->add(title);

    // ── Card containing the orange banner + override option + bottom row ──
    const int card_x = 40;
    const int card_y = 48;
    const int card_w = dt::SCREEN_W - 80;
    const int card_h = 350;

    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kBgWhite);
    card->border(1);
    card->color(Palette::ColorId::border, dt::kGrayLight);
    card->border_radius(12);
    container->add(card);

    // ── 1) Orange banner ───────────────────────────────────────────────────
    const int banner_h = 76;
    auto banner = make_shared<Frame>(Rect(0, 0, card_w, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, dt::kOrange);
    banner->border(0);
    banner->border_radius(12);
    card->add(banner);

    // Wi-Fi-off glyph inside a white round chip on the left
    const int chip_d = 48;
    auto chip = make_shared<Frame>(Rect(20, (banner_h - chip_d) / 2, chip_d, chip_d));
    chip->fill_flags({Theme::FillFlag::blend});
    chip->color(Palette::ColorId::bg, dt::kWhite);
    chip->border(0);
    chip->border_radius(chip_d / 2);
    banner->add(chip);
    auto chip_glyph = make_shared<WifiOffGlyph>(
        Rect(20, (banner_h - chip_d) / 2, chip_d, chip_d), dt::kTextPrimary);
    banner->add(chip_glyph);

    auto banner_l1 = make_shared<Label>("Wi-Fi Network not found.",
        Rect(20 + chip_d + 16, 14, card_w - (20 + chip_d + 16) - 16, 24));
    banner_l1->font(Font(16, Font::Weight::bold));
    banner_l1->color(Palette::ColorId::label_text, dt::kWhite);
    banner_l1->text_align(AlignFlag::left | AlignFlag::center_vertical);
    banner->add(banner_l1);

    auto banner_l2 = make_shared<Label>("No available network detected.",
        Rect(20 + chip_d + 16, 40, card_w - (20 + chip_d + 16) - 16, 24));
    banner_l2->font(Font(14, Font::Weight::normal));
    banner_l2->color(Palette::ColorId::label_text, dt::kWhite);
    banner_l2->text_align(AlignFlag::left | AlignFlag::center_vertical);
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

    auto retry = make_icon_button(
        30, row_y, row_w, row_h, "Retry WiFi",
        [](const Rect& r) { return make_shared<RefreshGlyph>(r, dt::kTextPrimary); },
        on_retry_wifi);
    card->add(retry.frame);

    auto setting = make_icon_button(
        30 + row_w + row_gap, row_y, row_w, row_h, "Setting",
        [](const Rect& r) { return make_shared<GearGlyph>(r, dt::kTextPrimary); },
        on_settings);
    card->add(setting.frame);

    // ── Back at bottom-left (shared chevron + label) ──────────────────────
    ui::add_back_button(*container, on_back);

    return container;
}
