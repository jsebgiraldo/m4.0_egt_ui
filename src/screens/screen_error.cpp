#include "screen_error.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <egt/svgimage.h>

using namespace egt;
using namespace std;

// ── Figma-matched layout (432×261 canvas → 800×480, dual scale) ──────────────
// Card: Figma Rectangle 56 @ x=21,y=20, 390x219. Header (Union) 390x95 blue.
// All children below are positioned relative to the card's top-left.
namespace {

constexpr int CARD_X = 39;    // 21 * 1.852
constexpr int CARD_Y = 37;    // 20 * 1.836
constexpr int CARD_W = 722;   // 390 * 1.852
constexpr int CARD_H = 402;   // 219 * 1.836
constexpr int CARD_R = 30;    // 16px * SCALE
constexpr int HEADER_H = 174; // 95 * 1.836

Image load_svg_icon(const string& path, int size)
{
    // libegt 1.10 (target) heap-corruption fix: keep SvgImage alive in a
    // static cache (see screen_patient_info.cpp load_svg_icon).
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        auto svg = std::make_shared<SvgImage>(("file:" + path).c_str(), SizeF(size, size));
        s_cache.push_back(svg);
        return static_cast<Image>(*svg);
    } catch (...) { return {}; }
}

// White action button (Figma "bt Pause/End": white card, gray bold label,
// soft shadow 0 2 2 rgba(0,0,0,0.2)). `sub` adds a smaller second line.
shared_ptr<Frame> make_error_button(const string& label, const string& sub,
                                     const Rect& rect, function<void()> on_click)
{
    constexpr int PAD = 8;
    auto wrap = make_shared<Frame>(
        Rect(rect.x() - PAD, rect.y() - PAD,
             rect.width() + 2 * PAD, rect.height() + 2 * PAD));
    wrap->fill_flags({});

    for (int i = 2; i >= 0; --i) {
        auto sh = make_shared<Frame>(
            Rect(PAD - i, PAD + 2 + i, rect.width() + 2 * i, rect.height() + 2 * i));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, 26));
        sh->border_radius(dt::RADIUS_XS + i);
        sh->border(0);
        wrap->add(sh);
    }

    auto f = make_shared<Frame>(Rect(PAD, PAD, rect.width(), rect.height()));
    f->fill_flags({Theme::FillFlag::blend});
    f->color(Palette::ColorId::bg, dt::kWhite);
    f->border(0);
    f->border_radius(dt::RADIUS_XS);

    if (sub.empty()) {
        auto l = make_shared<Label>(label,
            Rect(0, 0, rect.width(), rect.height()), AlignFlag::center);
        l->font(Font(26, Font::Weight::bold));   // Figma 14pt * SCALE
        l->color(Palette::ColorId::label_text, dt::kTextPrimary);
        f->add(l);
    } else {
        const int grp_top = (rect.height() - 46) / 2;
        auto l = make_shared<Label>(label,
            Rect(0, grp_top, rect.width(), 30), AlignFlag::center);
        l->font(Font(26, Font::Weight::bold));
        l->color(Palette::ColorId::label_text, dt::kTextPrimary);
        f->add(l);
        auto s = make_shared<Label>(sub,
            Rect(0, grp_top + 28, rect.width(), 20), AlignFlag::center);
        s->font(Font(19, Font::Weight::bold));    // Figma 10pt * SCALE
        s->color(Palette::ColorId::label_text, dt::kTextPrimary);
        f->add(s);
    }

    if (on_click)
        f->on_event([on_click](Event& e) {
            if (e.id() == EventId::pointer_click) on_click();
        }, {EventId::pointer_click});

    wrap->add(f);
    return wrap;
}

} // namespace

shared_ptr<Widget> create_error_screen(
    const string& icon_asset,
    const string& title,
    const string& message,
    const vector<ErrorAction>& actions,
    const Color& header_color)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Card drop shadow (Figma effect 4 4 8 rgba(0,0,0,0.3)): stacked, low-
    //    alpha rounded rects biased down-right so they read as a soft shadow.
    for (int i = 8; i >= 1; --i) {
        auto sh = make_shared<Frame>(
            Rect(CARD_X - i + 3, CARD_Y - i + 4, CARD_W + 2 * i, CARD_H + 2 * i));
        sh->fill_flags({Theme::FillFlag::blend});
        sh->color(Palette::ColorId::bg, Color(0, 0, 0, 9));
        sh->border_radius(CARD_R + i);
        sh->border(0);
        container->add(sh);
    }

    // ── Card (Figma Rectangle 56: keyboard-gray gradient -> near-white solid) ─
    auto card = make_shared<Frame>(Rect(CARD_X, CARD_Y, CARD_W, CARD_H));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, Color(0xF6, 0xF6, 0xF6));
    card->border(0);
    card->border_radius(CARD_R);
    container->add(card);

    // ── Blue header with rounded top, square bottom (Figma "Union") ──────────
    // A rounded rect for the top + a square rect over the bottom corners.
    auto header_round = make_shared<Frame>(Rect(0, 0, CARD_W, HEADER_H));
    header_round->fill_flags({Theme::FillFlag::blend});
    header_round->color(Palette::ColorId::bg, header_color);
    header_round->border(0);
    header_round->border_radius(CARD_R);
    card->add(header_round);

    auto header_sq = make_shared<Frame>(Rect(0, CARD_R, CARD_W, HEADER_H - CARD_R));
    header_sq->fill_flags({Theme::FillFlag::blend});
    header_sq->color(Palette::ColorId::bg, header_color);
    header_sq->border(0);
    card->add(header_sq);

    // ── White-disc icon, centred near the top of the header (Figma 40x40) ────
    const int icon_sz = 74;   // 40 * 1.852
    auto icon = load_svg_icon(icon_asset, icon_sz);
    if (!icon.empty()) {
        auto il = make_shared<ImageLabel>(icon);
        il->autoresize(false);
        il->border(0); il->padding(0); il->margin(0);
        il->fill_flags({});
        il->image_align(AlignFlag::center);
        il->box(Rect((CARD_W - icon_sz) / 2, 20, icon_sz, icon_sz));
        card->add(il);
    }

    // ── Title (white, bold), centred on the header below the icon ────────────
    auto lbl_title = make_shared<Label>(title,
        Rect(0, 112, CARD_W, 50), AlignFlag::center);
    lbl_title->font(Font(33, Font::Weight::bold));   // Figma 18pt * SCALE
    lbl_title->color(Palette::ColorId::label_text, dt::kWhite);
    card->add(lbl_title);

    // ── Body message (gray, bold), left-aligned below the header. One label
    //    per line: a single multi-line Label centres each line in EGT, but the
    //    Figma copy is left-aligned. Line height 20px * SCALE ≈ 37.
    {
        const int line_h = 37;
        int y = HEADER_H + 30;
        size_t start = 0;
        while (start <= message.size()) {
            const size_t nl = message.find('\n', start);
            const string line = message.substr(start,
                nl == string::npos ? string::npos : nl - start);
            auto lbl = make_shared<Label>(line,
                Rect(72, y, CARD_W - 110, line_h),
                AlignFlag::left | AlignFlag::center_vertical);
            lbl->font(Font(26, Font::Weight::bold));   // Figma 14pt * SCALE
            lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
            card->add(lbl);
            y += line_h;
            if (nl == string::npos) break;
            start = nl + 1;
        }
    }

    // ── Action buttons along the bottom of the card ──────────────────────────
    // Figma: Pause @x=64, middle @x=167, End @x=265 (card-relative after the
    // x=21 card origin), y≈186, ~98x40. Lay out up to three in those slots.
    struct Slot { int x, w; };
    static const Slot slots3[3] = { {80, 182}, {270, 172}, {452, 182} };
    const int btn_y = 305, btn_h = 73;
    for (size_t i = 0; i < actions.size() && i < 3; ++i) {
        const auto& a = actions[i];
        card->add(make_error_button(a.label, a.sublabel,
            Rect(slots3[i].x, btn_y, slots3[i].w, btn_h), a.on_click));
    }

    return container;
}
