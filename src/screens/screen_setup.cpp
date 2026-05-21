#include "screen_setup.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <egt/svgimage.h>
#include <fstream>
#include <memory>
#include <string>

using namespace egt;
using namespace std;

namespace {

// Same gear SVG used across the app (HOME / Settings / wifi-unavailable) so
// the icon language stays consistent.
static const char* kGearSvg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M19.44 12.99c.04-.33.07-.66.07-1s-.03-.67-.07-1l2.44-1.92-2.32-4-2.82 1.17c-.5-.37-1.04-.69-1.63-.94l-.37-3h-4.64l-.38 3c-.59.25-1.12.57-1.62.94l-2.82-1.17-2.32 4 2.44 1.92c-.04.33-.07.66-.07 1s.03.67.07 1l-2.44 1.92 2.32 4 2.82-1.17c.5.37 1.04.69 1.63.94l.38 3h4.64l.38-3c.59-.25 1.12-.57 1.62-.94l2.82 1.17 2.32-4-2.44-1.92zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z" fill="#646469"/>
</svg>)svg";

static Image load_gear(int size) {
    try {
        const string path = "/tmp/egt-icon-setup-gear.svg";
        ofstream f(path); f << kGearSvg; f.close();
        SvgImage svg("file:" + path, SizeF(size, size));
        return static_cast<Image>(svg);
    } catch (...) { return {}; }
}

} // namespace

shared_ptr<Widget> create_setup_screen(function<void()> on_settings)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // ── Logo, centred (upper-middle) ───────────────────────────────────────
    const int logo_w = 288, logo_h = 180;
    auto logo = ui::create_logo(
        (dt::SCREEN_W - logo_w) / 2, 110, logo_w, logo_h);
    container->add(logo);

    // ── "Settings" affordance (bottom-left): gray circle + gear + label ────
    const int circle_d = 46;
    const int circle_x = 27;
    const int circle_y = dt::SCREEN_H - 66;   // canonical bottom-left position
    auto circle = make_shared<Frame>(Rect(circle_x, circle_y, circle_d, circle_d));
    circle->fill_flags({Theme::FillFlag::blend});
    circle->color(Palette::ColorId::bg, palette::kGray200);
    circle->border(0);
    circle->border_radius(circle_d / 2);
    container->add(circle);

    auto gear = load_gear(26);
    if (!gear.empty()) {
        const int isz = 26;
        auto gl = make_shared<ImageLabel>(gear);
        gl->fill_flags({});
        gl->color(Palette::ColorId::bg, palette::kGray200);
        gl->image_align(AlignFlag::center);
        gl->move(Point(circle_x + (circle_d - isz) / 2,
                       circle_y + (circle_d - isz) / 2));
        gl->resize(Size(isz, isz));
        container->add(gl);
    }

    auto label = make_shared<Label>("Settings",
        Rect(circle_x + circle_d + 12, circle_y, 160, circle_d),
        AlignFlag::left | AlignFlag::center_vertical);
    label->font(Font(18, Font::Weight::bold));
    label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(label);

    // Tapping anywhere on the circle/label hit-zone opens Settings.
    auto hit = make_shared<Frame>(
        Rect(circle_x, circle_y, circle_d + 12 + 160, circle_d));
    hit->fill_flags({});
    hit->color(Palette::ColorId::bg, dt::kTransparent);
    hit->border(0);
    if (on_settings)
        hit->on_event([on_settings](Event&) { on_settings(); }, {EventId::pointer_click});
    container->add(hit);

    return container;
}
