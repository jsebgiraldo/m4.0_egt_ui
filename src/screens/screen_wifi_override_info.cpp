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

// X close glyph drawn with the Painter (two crossed strokes) so it renders
// on the target regardless of whether the device font has a ✕ glyph — the
// unicode character was coming up blank on the hardware.
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

// ── Continue button with vertical cyan→blue gradient ──────────────────────
// Matches Figma 2073:1808 — bg linear-gradient(#30a3c4 → #305fc4), rounded
// 4 px, "Continue" white Gothic A1 Bold centered. Same gradient family as
// the HOME Start button (palette::kStartTop / kStartBottom).
class ContinueGradientButton : public Widget {
public:
    ContinueGradientButton(const Rect& rect, function<void()> on_click)
        : Widget(rect), m_on_click(std::move(on_click)) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        on_event([this](Event& e) {
            switch (e.id()) {
                case EventId::raw_pointer_down: m_pressed = true;  damage(); break;
                case EventId::raw_pointer_up:   m_pressed = false; damage(); break;
                case EventId::pointer_click:    if (m_on_click) m_on_click(); break;
                default: break;
            }
        });
    }
    void draw(Painter& painter, const Rect&) override {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = 7.0f;

        // Drop shadow (Figma 0px 4px 4px rgba(0,0,0,0.2))
        rounded_rect(painter, x, y + 4.0f, w, h, r);
        painter.set(Color(0, 0, 0, 50));
        painter.fill();

        // Cyan→blue vertical gradient
        Color top = m_pressed ? Color(palette::kStartTopPress) : Color(palette::kStartTop);
        Color bot = m_pressed ? Color(palette::kStartBotPress) : Color(palette::kStartBottom);
        Pattern grad(Pattern::StepArray{{0.0f, top}, {1.0f, bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        rounded_rect(painter, x, y, w, h, r);
        painter.set(grad);
        painter.fill();

        // Centered white label, painted in-widget so it never intercepts the
        // click (same trick as StartButton).
        painter.set(Color(255, 255, 255));
        painter.set(Font(28, Font::Weight::bold));
        const auto ts = painter.text_size("Continue");
        painter.draw(PointF(x + (w - ts.width()) / 2.0f,
                            y + (h - ts.height()) / 2.0f));
        painter.draw(string("Continue"));
    }
private:
    bool m_pressed{false};
    function<void()> m_on_click;
    static void rounded_rect(Painter& p, float x, float y, float w, float h, float r) {
        const auto PI = static_cast<float>(M_PI);
        p.draw(PointF(x + r, y));
        p.line(PointF(x + w - r, y));
        p.draw(Arc(PointF(x + w - r, y + r), r, -PI / 2, 0.0f));
        p.line(PointF(x + w, y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f, PI / 2));
        p.line(PointF(x + r, y + h));
        p.draw(Arc(PointF(x + r, y + h - r), r, PI / 2, PI));
        p.line(PointF(x, y + r));
        p.draw(Arc(PointF(x + r, y + r), r, PI, 3 * PI / 2));
    }
};

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
    const int card_x = 40, card_y = 22, card_w = dt::SCREEN_W - 80, card_h = 446;
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
        Rect(0, banner_h + 24, 220, 76),
        AlignFlag::right | AlignFlag::center_vertical);
    num->font(Font(56, Font::Weight::bold));
    num->color(Palette::ColorId::label_text, dt::kGreen);
    card->add(num);

    // Single line (no wrap) per request.
    auto unit = make_shared<Label>("additional calendar " + day_word,
        Rect(232, banner_h + 24, card_w - 232 - 20, 76),
        AlignFlag::left | AlignFlag::center_vertical);
    unit->font(Font(18, Font::Weight::bold));
    unit->color(Palette::ColorId::label_text, dt::kGreen);
    card->add(unit);

    // ── Save-info text ──────────────────────────────────────────────────────
    auto save = make_shared<Label>(
        "It will still save all treatment information on device\n"
        "& transmit once Connection is established.",
        Rect(20, banner_h + 100, card_w - 40, 46), AlignFlag::center);
    save->font(Font(14, Font::Weight::normal));
    save->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(save);

    // ── Continue button (cyan→blue gradient) + (!) info badge ───────────────
    // Figma 2073:1808: 231×43.682 figma px at (100, 154) → 428×80 device px at
    // screen (185, 283). Card has padding (40, 22), so card-local = (145, 261).
    const int cont_w = 428, cont_h = 80;
    const int cont_x = 145;            // = 185 (screen) - card_x (40)
    const int cont_y = 261;            // = 283 (screen) - card_y (22)
    auto btn_continue = make_shared<ContinueGradientButton>(
        Rect(cont_x, cont_y, cont_w, cont_h), on_continue);
    card->add(btn_continue);

    // Figma 2079:2300 popup: bg rect 396×225 figma at local (16, 17) → device
    // 733×413 at (30, 31). Rounded 8 figma → 15 device. Color
    // rgba(100,101,105,0.9). Added DIRECTLY to container (not to card) since
    // it overlays the whole screen and the bottom row is meant to be drawn
    // on top of it (added after).
    const int popup_x = 30;
    const int popup_y = 31;
    const int popup_w = 740;           // 733 figma + small symmetric rounding
    const int popup_h = 413;
    auto popup = make_shared<Frame>(Rect(popup_x, popup_y, popup_w, popup_h));
    popup->fill_flags({Theme::FillFlag::blend});
    popup->color(Palette::ColorId::bg, Color(100, 101, 105, 230));
    popup->border(0);
    popup->border_radius(15);

    auto badge = make_shared<InfoBadge>(
        Rect(cont_x + cont_w + 16, cont_y + (cont_h - 36) / 2, 36, 36),
        [popup]() { popup->show(); });
    card->add(badge);

    // ── Bottom row: Back / Retry WiFi / Setting (added BEFORE popup so the
    // dark overlay dims them per Figma 2079:2300) ──────────────────────────
    const int row_y = banner_h + 262;
    const int row_h = 60, row_gap = 14;
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

    // ── Info popup content (sits inside the rounded popup card) ───────────
    // All coordinates here are LOCAL to the popup Frame. Figma 2073:1850:
    // text box 310×160 figma at frame-local (67, 51) → popup-local (51, 34)
    // → device popup-local (94, 62). Font 16 figma px ≈ 23 device, leading
    // 20 figma ≈ 37 device.
    {
        // Orange info icon top-left (Figma 2073:1852: 21×21.5 at frame-local
        // (26, 24) → popup-local (10, 7) → device (18, 13)).
        try {
            auto img = Image("file:assets/figma/images/wifi-info-icon.png");
            auto info_lbl = make_shared<ImageLabel>(img);
            info_lbl->autoresize(false);
            info_lbl->border(0); info_lbl->padding(0); info_lbl->margin(0);
            info_lbl->fill_flags({});
            info_lbl->image_align(AlignFlag::center);
            info_lbl->box(Rect(18, 13, 44, 44));
            popup->add(info_lbl);
        } catch (...) { /* fall back to no icon */ }

        // X close (Figma 2073:1861: 15×15 at frame-local (380, 28) →
        // popup-local (364, 11) → device (674, 20)). Drawn larger (44×44)
        // than Figma for a usable touch target. Per Figma 2079:2300, X
        // dismisses the popup to reveal the underlying card (with the blue
        // Continue button and Back/Retry/Setting row).
        popup->add(make_shared<CloseX>(
            Rect(popup_w - 64, 14, 44, 44), dt::kWhite,
            [popup]() { popup->hide(); }));

        // Body text — Figma 2073:1850 is a single text node with an inline
        // green span on "7 calendar day(s)". Without rich-text we split into
        // sequential Labels and put the green phrase on its own line.
        const int M  = 94;                  // left margin (device from popup edge)
        const int tw = popup_w - 2 * M;     // text width (~552)

        const int y0 = 70;                  // first text line baseline
        auto l1 = make_shared<Label>("Please note that on",
            Rect(M, y0, tw, 38), AlignFlag::center);
        l1->font(Font(22)); l1->color(Palette::ColorId::label_text, dt::kWhite);
        popup->add(l1);

        auto l2 = make_shared<Label>(to_string(days) + " calendar day(s) from today,",
            Rect(M, y0 + 44, tw, 38), AlignFlag::center);
        l2->font(Font(22, Font::Weight::bold));
        l2->color(Palette::ColorId::label_text, dt::kGreen);
        popup->add(l2);

        auto l3 = make_shared<Label>(
            "a Wi-Fi/Network Connection must be established,\n"
            "or an Override Password must be entered for the\n"
            "device to continue to operate.",
            Rect(M, y0 + 90, tw, 110), AlignFlag::center);
        l3->font(Font(20)); l3->color(Palette::ColorId::label_text, dt::kWhite);
        popup->add(l3);

        auto l4 = make_shared<Label>(
            "(An Override Password is provided by Larada Sciences,\n"
            "please contact your Clinic Success contact for more details).",
            Rect(M, y0 + 215, tw, 68), AlignFlag::center);
        l4->font(Font(17)); l4->color(Palette::ColorId::label_text, palette::kGray200);
        popup->add(l4);
    }
    container->add(popup);

    return container;
}
