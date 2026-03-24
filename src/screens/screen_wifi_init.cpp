#include "screen_wifi_init.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../wifi/wifi_backend.h"
#include <cmath>
#include <chrono>
#include <cstdlib>

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
        constexpr int segments = 120;
        constexpr float arc_span = 5.2f; // ~300° in radians
        constexpr float seg_angle = arc_span / segments;
        constexpr uint8_t gr = 91, gg = 197, gb = 0;

        painter.line_width(linew);
        for (int i = 0; i < segments; i++) {
            float t = static_cast<float>(i) / (segments - 1);
            auto alpha = static_cast<uint8_t>(t * 255.0f);
            float seg_start = m_angle - arc_span + i * seg_angle;

            painter.set(Color(gr, gg, gb, alpha));
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

    // Figma: Spinner COMPONENT 214×214 → ~396px scaled, centered vertically
    const int spin_sz = 396;
    const int spin_x = (dt::SCREEN_W - spin_sz) / 2;
    const int spin_y = (dt::SCREEN_H - spin_sz) / 2;

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

    // Status text (centered inside ring, below logo)
    auto status_label = make_shared<Label>("Checking WiFi...",
        Rect(spin_x, spin_y + 230, spin_sz, 40));
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

    // State for WiFi polling
    auto poll_count = make_shared<int>(0);
    const bool mock_mode = (std::getenv("EGT_MOCK_WIFI") != nullptr);
    const int MAX_POLLS = mock_mode ? 2 : 15;   // mock: 4s, real: 30s
    const int POLL_MS   = 2000;

    // WiFi check timer — polls every 2s
    auto wifi_timer = make_shared<PeriodicTimer>(chrono::milliseconds(POLL_MS));
    wifi_timer->on_timeout([=]() {
        (*poll_count)++;
        printf("[WIFI_INIT] poll %d/%d\n", *poll_count, MAX_POLLS);
        fflush(stdout);

        egt_wifi::WiFiManager wifi;
        std::string ssid = wifi.get_current_ssid();

        if (!ssid.empty()) {
            // Connected!
            printf("[WIFI_INIT] connected to '%s'\n", ssid.c_str());
            fflush(stdout);
            status_label->text("Connected to " + ssid);
            wifi_timer->cancel();

            // Brief pause to show the "Connected" message, then advance
            auto done_timer = make_shared<PeriodicTimer>(chrono::milliseconds(1500));
            done_timer->on_timeout([=]() {
                done_timer->cancel();
                anim_timer->cancel();
                if (on_connected) on_connected();
            });
            done_timer->start();
        } else if (*poll_count >= MAX_POLLS) {
            // Timed out — no WiFi connection
            printf("[WIFI_INIT] timeout, no WiFi connection\n");
            fflush(stdout);
            status_label->text("No WiFi found");
            wifi_timer->cancel();

            // Brief pause to show the failure message, then go to settings
            auto fail_timer = make_shared<PeriodicTimer>(chrono::milliseconds(1500));
            fail_timer->on_timeout([=]() {
                fail_timer->cancel();
                anim_timer->cancel();
                if (on_failed) on_failed();
            });
            fail_timer->start();
        } else {
            status_label->text("Connecting to WiFi...");
        }
    });

    // Initial immediate check (after a short delay to let the UI render)
    auto initial_timer = make_shared<PeriodicTimer>(chrono::milliseconds(500));
    initial_timer->on_timeout([=]() {
        initial_timer->cancel();

        printf("[WIFI_INIT] initial check\n");
        fflush(stdout);

        egt_wifi::WiFiManager wifi;
        std::string ssid = wifi.get_current_ssid();

        if (!ssid.empty()) {
            // Already connected — go straight through
            printf("[WIFI_INIT] already connected to '%s'\n", ssid.c_str());
            fflush(stdout);
            status_label->text("Connected to " + ssid);

            auto done_timer = make_shared<PeriodicTimer>(chrono::milliseconds(1500));
            done_timer->on_timeout([=]() {
                done_timer->cancel();
                anim_timer->cancel();
                if (on_connected) on_connected();
            });
            done_timer->start();
        } else {
            // Not connected yet — start polling
            status_label->text("Connecting to WiFi...");
            wifi_timer->start();
        }
    });
    initial_timer->start();

    return container;
}
