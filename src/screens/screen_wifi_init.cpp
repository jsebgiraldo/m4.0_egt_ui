#include "screen_wifi_init.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <cmath>
#include <chrono>

using namespace egt;
using namespace std;

// Custom spinner ring widget with gradient arc (fades from solid green to transparent)
class SpinnerRing : public Widget {
public:
    explicit SpinnerRing(const Rect& rect)
        : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void angle(float a) { m_angle = a; damage(); }

    void draw(Painter& painter, const Rect& rect) override
    {
        auto b = content_area();
        auto dim = static_cast<float>(min(b.width(), b.height()));
        constexpr float linew = 14.0f;
        float radius = dim / 2.0f - linew / 2.0f;
        auto center = b.center();
        constexpr float twopi = 2.0f * static_cast<float>(M_PI);

        // Faint full-circle track
        painter.line_width(linew);
        painter.set(Color(dt::kGrayLight, 80));
        painter.draw(Arc(center, radius, 0.0f, twopi));
        painter.stroke();

        // Gradient arc: ~300° total, drawn as many small segments
        // Head (leading edge) at m_angle = full green
        // Tail fades to alpha=0 over the arc length
        constexpr int segments = 120;
        constexpr float arc_span = 5.2f; // ~300° in radians
        constexpr float seg_angle = arc_span / segments;
        // Green color components from kGreen (#5BC500)
        constexpr uint8_t gr = 91, gg = 197, gb = 0;

        painter.line_width(linew);
        for (int i = 0; i < segments; i++) {
            // i=0 is the tail (transparent), i=segments-1 is the head (opaque)
            float t = static_cast<float>(i) / (segments - 1);
            auto alpha = static_cast<uint8_t>(t * 255.0f);
            float seg_start = m_angle - arc_span + i * seg_angle;

            painter.set(Color(gr, gg, gb, alpha));
            // Draw slightly overlapping segment to avoid gaps
            painter.draw(Arc(center, radius, seg_start, seg_start + seg_angle + 0.02f));
            painter.stroke();
        }
    }

private:
    float m_angle{0.0f};
};

shared_ptr<Widget> create_wifi_init_screen(
    function<void()> on_connected,
    function<void()> on_failed)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Figma: Spinner COMPONENT 214×214 → ~396px scaled
    const int spin_sz = 380;
    const int spin_x = (dt::SCREEN_W - spin_sz) / 2;
    const int spin_y = 20;

    auto spinner = make_shared<SpinnerRing>(Rect(spin_x, spin_y, spin_sz, spin_sz));
    container->add(spinner);

    // Logo (centered inside the ring)
    const int logo_w = 200;
    const int logo_h = 125;
    auto logo = ui::create_logo(
        spin_x + (spin_sz - logo_w) / 2,
        spin_y + 70,
        logo_w,
        logo_h);
    container->add(logo);

    // "Connecting to Wifi" text (centered inside ring, below logo)
    auto status_label = make_shared<Label>("Connecting to Wifi",
        Rect(spin_x, spin_y + 230, spin_sz, 40));
    status_label->align(AlignFlag::center);
    status_label->font(Font(18, Font::Weight::normal));
    status_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status_label);

    // Animate: rotate spinner arc
    auto angle = make_shared<float>(0.0f);
    auto anim_timer = make_shared<PeriodicTimer>(chrono::milliseconds(30));
    anim_timer->on_timeout([spinner, angle]() {
        *angle += 0.08f;
        if (*angle > 2.0f * static_cast<float>(M_PI))
            *angle -= 2.0f * static_cast<float>(M_PI);
        spinner->angle(*angle);
    });
    anim_timer->start();

    // Auto-advance after 3 seconds
    auto advance_timer = make_shared<PeriodicTimer>(chrono::milliseconds(3000));
    advance_timer->on_timeout([anim_timer, advance_timer, on_connected]() {
        anim_timer->cancel();
        advance_timer->cancel();
        if (on_connected) on_connected();
    });
    advance_timer->start();

    return container;
}
