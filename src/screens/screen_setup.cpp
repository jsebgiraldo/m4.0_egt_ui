#include "screen_setup.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../ui/palette.h"

#include <egt/svgimage.h>
#include <fstream>
#include <memory>
#include <vector>
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

// NOTE: the Settings affordance now uses the embedded gear PNG (see below),
// not this SVG loader — kept only as a fallback. SvgImage is cached so the
// sliced Image never dangles on libegt 1.10 (target).
[[maybe_unused]] static Image load_gear(int size) {
    static std::vector<std::shared_ptr<SvgImage>> s_cache;
    try {
        const string path = "/tmp/egt-icon-setup-gear.svg";
        ofstream f(path); f << kGearSvg; f.close();
        auto svg = std::make_shared<SvgImage>("file:" + path, SizeF(size, size));
        s_cache.push_back(svg);
        return static_cast<Image>(*svg);
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

    // ── "Settings" affordance (bottom-left): gear PNG + label ──────────────
    // Plain ImageLabel + Label + transparent hit-zone. Avoid SvgImage and
    // border_radius — both have heap-corruption regressions on libegt 1.10
    // (the target's version) and were the root of the SETUP free() crash.
    const int circle_d = 46;
    const int circle_x = 27;
    const int circle_y = dt::SCREEN_H - 66;   // canonical bottom-left position

    try {
        auto img = Image(("file:" + ui::asset_path("wifi-settings-gear")).c_str());
        auto gl = make_shared<ImageLabel>(img);
        gl->autoresize(false);
        gl->border(0); gl->padding(0); gl->margin(0);
        gl->fill_flags({});
        gl->image_align(AlignFlag::center);
        gl->box(Rect(circle_x, circle_y, circle_d, circle_d));
        container->add(gl);
    } catch (...) { /* skip icon on error — never crash boot */ }

    auto label = make_shared<Label>("Settings",
        Rect(circle_x + circle_d + 12, circle_y, 160, circle_d),
        AlignFlag::left | AlignFlag::center_vertical);
    label->font(Font(18, Font::Weight::bold));
    label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(label);

    // Tapping anywhere on the icon/label hit-zone opens Settings.
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
