#include "screen_wifi_connected.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <cmath>

using namespace egt;
using namespace std;

// Green circle with a white checkmark — Figma "Connected" state icon.
class CheckCircle : public Widget {
public:
    explicit CheckCircle(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        auto dim = static_cast<float>(min(b.width(), b.height()));
        auto center = b.center();
        const float radius = dim / 2.0f - 4.0f;

        // Outline ring (stroked, not filled — matches Figma)
        painter.line_width(6.0f);
        painter.set(dt::kGreen);
        painter.draw(Arc(center, radius, 0.0f, 2.0f * static_cast<float>(M_PI)));
        painter.stroke();

        // Check mark — two-segment stroke
        const float cx = center.x();
        const float cy = center.y();
        const float r  = dim * 0.30f;   // half-width of check shape
        const Point p1(cx - r,        cy + r * 0.05f);
        const Point p2(cx - r * 0.30f, cy + r * 0.55f);
        const Point p3(cx + r * 0.95f, cy - r * 0.55f);

        painter.line_width(8.0f);
        painter.set(dt::kGreen);
        painter.draw(Line(p1, p2));
        painter.stroke();
        painter.draw(Line(p2, p3));
        painter.stroke();
    }
};

shared_ptr<Widget> create_wifi_connected_screen(
    function<void()> on_continue)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo top-left (compact — matches Figma small logo placement)
    const int logo_w = 110;
    const int logo_h = 68;
    auto logo = ui::create_logo(28, 24, logo_w, logo_h);
    container->add(logo);

    // Green check circle (centered horizontally, upper-middle)
    const int check_sz = 130;
    auto check = make_shared<CheckCircle>(
        Rect((dt::SCREEN_W - check_sz) / 2, 130, check_sz, check_sz));
    container->add(check);

    // Title "Wi-Fi Connected" — green, bold, centered
    auto title = make_shared<Label>("Wi-Fi Connected",
        Rect(0, 280, dt::SCREEN_W, 44));
    title->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kGreen);
    title->text_align(AlignFlag::center);
    container->add(title);

    // Subtitle — two lines, gray, centered
    auto sub = make_shared<Label>(
        "WiFi connection established successfully.\nThe device is ready to use.",
        Rect(0, 330, dt::SCREEN_W, 60));
    sub->font(Font(dt::FONT_BODY, Font::Weight::normal));
    sub->color(Palette::ColorId::label_text, dt::kTextPrimary);
    sub->text_align(AlignFlag::center);
    container->add(sub);

    // Continue button — outlined, with chevron, centered at bottom
    const int btn_w = 220;
    const int btn_h = 60;
    auto btn = ui::create_outlined_button(
        "Continue  >",
        Rect((dt::SCREEN_W - btn_w) / 2, dt::SCREEN_H - btn_h - 30, btn_w, btn_h),
        std::move(on_continue));
    container->add(btn);

    return container;
}
