#include "screen_wifi_override_info.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"
#include "../ui/override_config.h"

#include <egt/svgimage.h>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

namespace {

// ── Glyphs ──────────────────────────────────────────────────────────────────
// Wi-Fi-with-slash glyph (white) for the orange banner chip.
class WifiOffGlyph : public Widget {
public:
    WifiOffGlyph(const Rect& rect, const Color& col) : Widget(rect), m_col(col) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override {
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
        for (float r : radii) { painter.draw(Arc(pivot, r, start, end)); painter.stroke(); }
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

// "!" in a circle — the info badge (orange disc, white "!").
class InfoBadge : public Widget {
public:
    InfoBadge(const Rect& rect, function<void()> on_click)
        : Widget(rect), m_on_click(std::move(on_click)) {
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
        const float r  = sz * 0.45f;
        painter.set(dt::kOrange);
        painter.draw(Arc(PointF(cx, cy), r, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
        painter.set(Color(255, 255, 255));
        painter.line_width(std::max(2.5f, sz * 0.10f));
        painter.draw(Line(PointF(cx, cy - sz * 0.20f), PointF(cx, cy + sz * 0.08f)));
        painter.stroke();
        painter.draw(Arc(PointF(cx, cy + sz * 0.22f), sz * 0.055f,
                         0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.fill();
    }
private:
    function<void()> m_on_click;
};

// Bottom-strip icon button (gray circle + glyph + label) — mirrors the
// wifi-unavailable screen.
static const char* kGearSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M19.44 12.99c.04-.33.07-.66.07-1s-.03-.67-.07-1l2.44-1.92-2.32-4-2.82 1.17c-.5-.37-1.04-.69-1.63-.94l-.37-3h-4.64l-.38 3c-.59.25-1.12.57-1.62.94l-2.82-1.17-2.32 4 2.44 1.92c-.04.33-.07.66-.07 1s.03.67.07 1l-2.44 1.92 2.32 4 2.82-1.17c.5.37 1.04.69 1.63.94l.38 3h4.64l.38-3c.59-.25 1.12-.57 1.62-.94l2.82 1.17 2.32-4-2.44-1.92zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z" fill="#646469"/>
</svg>)svg";
static const char* kRefreshSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.74 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z" fill="#646469"/>
</svg>)svg";
static const char* kChevronSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M15.41 7.41 14 6l-6 6 6 6 1.41-1.41L10.83 12z" fill="#646469"/>
</svg>)svg";

static Image load_icon(const char* name, const char* svg, int size) {
    try {
        string path = string("/tmp/egt-ovi-") + name + ".svg";
        ofstream f(path); f << svg; f.close();
        SvgImage img("file:" + path, SizeF(size, size));
        return static_cast<Image>(img);
    } catch (...) { return {}; }
}

shared_ptr<Frame> make_icon_button(int x, int y, int w, int h,
                                   const string& label, const Image& icon,
                                   function<void()> on_click) {
    auto frame = make_shared<Frame>(Rect(x, y, w, h));
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, dt::kBgWhite);
    frame->border(1);
    frame->color(Palette::ColorId::border, dt::kGrayLight);
    frame->border_radius(8);
    const int cd = 36, cx = 14, cy = (h - cd) / 2;
    auto bg = make_shared<Frame>(Rect(cx, cy, cd, cd));
    bg->fill_flags({Theme::FillFlag::blend});
    bg->color(Palette::ColorId::bg, palette::kGray200);
    bg->border(0); bg->border_radius(cd / 2);
    frame->add(bg);
    if (!icon.empty()) {
        const int isz = 22;
        auto il = make_shared<ImageLabel>(icon);
        il->fill_flags({});
        il->color(Palette::ColorId::bg, palette::kGray200);
        il->image_align(AlignFlag::center);
        il->move(Point(cx + (cd - isz) / 2, cy + (cd - isz) / 2));
        il->resize(Size(isz, isz));
        frame->add(il);
    }
    auto lbl = make_shared<Label>(label,
        Rect(cx + cd + 12, 0, w - (cx + cd + 12) - 8, h));
    lbl->font(Font(15, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    frame->add(lbl);
    if (on_click)
        frame->on_event([on_click](Event&){ on_click(); }, {EventId::pointer_click});
    return frame;
}

} // namespace

shared_ptr<Widget> create_wifi_override_info_screen(
    function<void()> on_continue,
    function<void()> on_back,
    function<void()> on_retry_wifi,
    function<void()> on_settings)
{
    const int days = ui::get_override_days();
    const string day_word = (days == 1) ? "day(s)" : "day(s)";

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Outer card ─────────────────────────────────────────────────────────
    const int card_x = 40, card_y = 30, card_w = dt::SCREEN_W - 80, card_h = 420;
    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kBgWhite);
    card->border(1);
    card->color(Palette::ColorId::border, dt::kGrayLight);
    card->border_radius(12);
    container->add(card);

    // ── Orange banner ──────────────────────────────────────────────────────
    const int banner_h = 96;
    auto banner = make_shared<Frame>(Rect(0, 0, card_w, banner_h));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, dt::kOrange);
    banner->border(0); banner->border_radius(12);
    card->add(banner);

    const int chip_d = 56, chip_x = 20, chip_y = (banner_h - chip_d) / 2;
    auto chip = make_shared<Frame>(Rect(chip_x, chip_y, chip_d, chip_d));
    chip->fill_flags({Theme::FillFlag::blend});
    chip->color(Palette::ColorId::bg, dt::kWhite);
    chip->border(0); chip->border_radius(chip_d / 2);
    banner->add(chip);
    banner->add(make_shared<WifiOffGlyph>(
        Rect(chip_x, chip_y, chip_d, chip_d), dt::kTextPrimary));

    auto banner_txt = make_shared<Label>(
        "Wi-Fi/Network Connection remains unavailable,\n"
        "device will continue to operate normally for",
        Rect(chip_x + chip_d + 14, 0, card_w - (chip_x + chip_d + 14) - 20, banner_h),
        AlignFlag::center_vertical | AlignFlag::left);
    banner_txt->font(Font(16, Font::Weight::bold));
    banner_txt->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(banner_txt);

    // ── Big N + "additional calendar day(s)" ───────────────────────────────
    auto num = make_shared<Label>(to_string(days),
        Rect(0, banner_h + 8, 220, 70),
        AlignFlag::right | AlignFlag::center_vertical);
    num->font(Font(54, Font::Weight::bold));
    num->color(Palette::ColorId::label_text, dt::kGreen);
    card->add(num);

    auto unit = make_shared<Label>("additional calendar\n" + day_word,
        Rect(232, banner_h + 8, card_w - 232 - 20, 70),
        AlignFlag::left | AlignFlag::center_vertical);
    unit->font(Font(18, Font::Weight::bold));
    unit->color(Palette::ColorId::label_text, dt::kGreen);
    card->add(unit);

    // ── Save-info text ──────────────────────────────────────────────────────
    auto save = make_shared<Label>(
        "It will still save all treatment information on device\n"
        "& transmit once Connection is established.",
        Rect(20, banner_h + 86, card_w - 40, 50), AlignFlag::center);
    save->font(Font(14, Font::Weight::normal));
    save->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(save);

    // ── Continue button (blue) + (!) info badge ─────────────────────────────
    const int cont_w = 300, cont_h = 56;
    const int cont_x = (card_w - cont_w) / 2 - 20;
    const int cont_y = banner_h + 150;
    auto btn_continue = ui::create_filled_button("Continue",
        Rect(cont_x, cont_y, cont_w, cont_h), on_continue);
    card->add(btn_continue);

    // Info popup (built below) toggled by the badge.
    auto popup = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    popup->fill_flags({Theme::FillFlag::blend});
    popup->color(Palette::ColorId::bg, Color(40, 40, 40, 200));
    popup->border(0);
    popup->hide();

    auto badge = make_shared<InfoBadge>(
        Rect(cont_x + cont_w + 16, cont_y + (cont_h - 36) / 2, 36, 36),
        [popup]() { popup->show(); });
    card->add(badge);

    // ── Bottom row: Back / Retry WiFi / Setting ─────────────────────────────
    const int row_y = banner_h + 224;
    const int row_h = 56, row_gap = 14;
    const int row_w = (card_w - 30 * 2 - 2 * row_gap) / 3;
    auto back_icon  = load_icon("chev",    kChevronSvg, 22);
    auto rfsh_icon  = load_icon("refresh", kRefreshSvg, 22);
    auto gear_icon  = load_icon("gear",    kGearSvg,    22);
    card->add(make_icon_button(30, row_y, row_w, row_h,
        "Back", back_icon, on_back));
    card->add(make_icon_button(30 + row_w + row_gap, row_y, row_w, row_h,
        "Retry WiFi", rfsh_icon, on_retry_wifi));
    card->add(make_icon_button(30 + 2 * (row_w + row_gap), row_y, row_w, row_h,
        "Setting", gear_icon, on_settings));

    // ── Info popup overlay (Figma right screen) ─────────────────────────────
    {
        const int pc_w = 560, pc_h = 300;
        auto pcard = make_shared<Frame>(
            Rect((dt::SCREEN_W - pc_w) / 2, (dt::SCREEN_H - pc_h) / 2, pc_w, pc_h));
        pcard->fill_flags({Theme::FillFlag::blend});
        pcard->color(Palette::ColorId::bg, Color(60, 60, 60, 235));
        pcard->border(0); pcard->border_radius(12);
        popup->add(pcard);

        // X close (top-right)
        auto close = make_shared<Label>("✕",
            Rect(pc_w - 52, 10, 42, 42), AlignFlag::center);
        close->font(Font(26, Font::Weight::bold));
        close->color(Palette::ColorId::label_text, dt::kWhite);
        close->on_event([popup](Event&){ popup->hide(); }, {EventId::pointer_click});
        pcard->add(close);

        // Body text with the green "N calendar day(s)" inline is hard in one
        // Label; render as three stacked lines, the day count line green.
        auto l1 = make_shared<Label>("Please note that on",
            Rect(36, 40, pc_w - 72, 30), AlignFlag::left | AlignFlag::center_vertical);
        l1->font(Font(17)); l1->color(Palette::ColorId::label_text, dt::kWhite);
        pcard->add(l1);

        auto l2 = make_shared<Label>(to_string(days) + " calendar day(s) from today,",
            Rect(36, 68, pc_w - 72, 30), AlignFlag::left | AlignFlag::center_vertical);
        l2->font(Font(17, Font::Weight::bold));
        l2->color(Palette::ColorId::label_text, dt::kGreen);
        pcard->add(l2);

        auto l3 = make_shared<Label>(
            "a Wi-Fi/Network Connection must be established,\n"
            "or an Override Password must be entered for the\n"
            "device to continue to operate.",
            Rect(36, 100, pc_w - 72, 76), AlignFlag::left);
        l3->font(Font(16)); l3->color(Palette::ColorId::label_text, dt::kWhite);
        pcard->add(l3);

        auto l4 = make_shared<Label>(
            "(An Override Password is provided by Larada\n"
            "Sciences, please contact your Clinic Success\n"
            "contact for more details).",
            Rect(36, 184, pc_w - 72, 76), AlignFlag::left);
        l4->font(Font(15)); l4->color(Palette::ColorId::label_text, palette::kGray200);
        pcard->add(l4);
    }
    container->add(popup);

    return container;
}
