#include <egt/ui>
#include <egt/svgimage.h>
#include "screen_home.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <fstream>
#include <string>

using namespace egt;
using namespace std;

// ── SVG icon helpers ─────────────────────────────────────────────────────────
static string write_svg_tmp(const char* name, const char* svg_data)
{
    string path = string("/tmp/egt-icon-") + name + ".svg";
    static bool written_person = false;
    static bool written_gear   = false;
    bool& written = (string(name) == "person") ? written_person : written_gear;
    if (!written) {
        ofstream f(path);
        f << svg_data;
        written = f.good();
    }
    return path;
}

// Person silhouette — kTextPrimary fill (#646469)
static const char* kPersonSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 12c2.76 0 5-2.24 5-5s-2.24-5-5-5-5 2.24-5 5 2.24 5 5 5zm0 2c-3.33 0-10 1.67-10 5v2h20v-2c0-3.33-6.67-5-10-5z" fill="#646469"/>
</svg>)svg";

// Gear / settings — kTextPrimary fill (#646469)
static const char* kGearSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M19.44 12.99c.04-.33.07-.66.07-1s-.03-.67-.07-1l2.44-1.92-2.32-4-2.82 1.17c-.5-.37-1.04-.69-1.63-.94l-.37-3h-4.64l-.38 3c-.59.25-1.12.57-1.62.94l-2.82-1.17-2.32 4 2.44 1.92c-.04.33-.07.66-.07 1s.03.67.07 1l-2.44 1.92 2.32 4 2.82-1.17c.5.37 1.04.69 1.63.94l.38 3h4.64l.38-3c.59-.25 1.12-.57 1.62-.94l2.82 1.17 2.32-4-2.44-1.92zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z" fill="#646469"/>
</svg>)svg";

// Load an SVG and return an Image at the given size (returns empty on error)
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

// ── Build a card button (white bg, gray border, icon + label) ─────────────────
static shared_ptr<Frame> make_card_button(
    const Rect& rect,
    const Image& icon,
    const string& text,
    function<void()> on_click)
{
    auto frame = make_shared<Frame>(rect);
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, dt::kWhite);
    frame->border(2);
    frame->color(Palette::ColorId::border, dt::kGrayLight);
    frame->border_radius(dt::RADIUS_MD);

    const int icon_sz  = 36;
    const int icon_x   = 22;
    const int icon_y   = (rect.height() - icon_sz) / 2;
    const int label_x  = icon_x + icon_sz + 12;
    const int label_w  = rect.width() - label_x - 10;

    if (!icon.empty()) {
        auto icon_lbl = make_shared<ImageLabel>(icon);
        icon_lbl->fill_flags({Theme::FillFlag::blend});
        icon_lbl->image_align(AlignFlag::center);
        icon_lbl->move(Point(icon_x, icon_y));
        icon_lbl->resize(Size(icon_sz, icon_sz));
        frame->add(icon_lbl);
    }

    auto lbl = make_shared<Label>(text, Rect(label_x, 0, label_w, rect.height()));
    lbl->font(Font(17, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    frame->add(lbl);

    if (on_click)
        frame->on_event([on_click](Event&) { on_click(); }, {EventId::pointer_click});

    return frame;
}

// ────────────────────────────────────────────────────────────────────────────
shared_ptr<Widget> create_home_screen(
    function<void()> on_begin_treatment,
    function<void()> on_demo_mode,
    function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo (centered, upper portion) ──────────────────────────────────────
    // Figma Group 197: logo centered at top ~y=30
    const int logo_w = 150, logo_h = 94;
    auto logo = ui::create_logo(
        (dt::SCREEN_W - logo_w) / 2,
        28,
        logo_w,
        logo_h);
    container->add(logo);

    // ── "Begin Treatment" button (large, centered, green) ───────────────────
    // Figma: "Begin" (large bold) + "Treatment" (smaller) — two distinct sizes
    const int btn_w = 300, btn_h = 138;
    const int btn_x = (dt::SCREEN_W - btn_w) / 2;
    const int btn_y = 146; // moved up from 180 — tighter to logo

    auto btn_begin = make_shared<Frame>(Rect(btn_x, btn_y, btn_w, btn_h));
    btn_begin->fill_flags({Theme::FillFlag::blend});
    btn_begin->color(Palette::ColorId::bg, dt::kAccentCyan);
    btn_begin->border(0);
    btn_begin->border_radius(dt::RADIUS_MD);

    // "Start" — large bold, single word as per Figma HOME
    auto lbl_start = make_shared<Label>("Start",
        Rect(0, 0, btn_w, btn_h));
    lbl_start->font(Font(38, Font::Weight::bold));
    lbl_start->color(Palette::ColorId::label_text, dt::kWhite);
    lbl_start->text_align(AlignFlag::center);
    btn_begin->add(lbl_start);

    // Press feedback: slightly darken on down
    auto orig_cyan = dt::kAccentCyan;
    Color pressed_cyan(30, 130, 160);
    btn_begin->on_event([=](Event& e) {
        if (e.id() == EventId::raw_pointer_down) {
            btn_begin->color(Palette::ColorId::bg, pressed_cyan);
            btn_begin->damage();
        } else if (e.id() == EventId::raw_pointer_up) {
            btn_begin->color(Palette::ColorId::bg, orig_cyan);
            btn_begin->damage();
        }
    });
    btn_begin->on_event([=](Event&) { on_begin_treatment(); }, {EventId::pointer_click});

    container->add(btn_begin);

    // ── Bottom card buttons ──────────────────────────────────────────────────
    // Figma: two outlined cards at bottom — "Demo Mode" left, "Setting" right
    const int card_w = 345;
    const int card_h = 100;
    const int card_y = 315; // moved up from dt::SCREEN_H - card_h - 15 = 369

    // Load SVG icons
    auto icon_person = load_svg_icon("person", kPersonSvg, 36);
    auto icon_gear   = load_svg_icon("gear",   kGearSvg,   36);

    // "Demo Mode" (bottom-left)
    auto btn_demo = make_card_button(
        Rect(30, card_y, card_w, card_h),
        icon_person,
        "Demo Mode",
        on_demo_mode);
    container->add(btn_demo);

    // "Setting" (bottom-right)
    auto btn_setting = make_card_button(
        Rect(dt::SCREEN_W - card_w - 30, card_y, card_w, card_h),
        icon_gear,
        "Settings",
        on_settings);
    container->add(btn_setting);

    return container;
}
