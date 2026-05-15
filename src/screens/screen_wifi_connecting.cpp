#include "screen_wifi_connecting.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../wifi/wifi_backend.h"
#include <cmath>
#include <atomic>
#include <thread>
#include <memory>

using namespace egt;
using namespace std;

// Rotating arc spinner — same visual language as the wifi_init spinner but
// sized for this transient screen.
class ConnectSpinner : public Widget {
public:
    explicit ConnectSpinner(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void angle(float a) { m_angle = a; damage(); }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        auto dim = static_cast<float>(min(b.width(), b.height()));
        constexpr float linew = 10.0f;
        float radius = dim / 2.0f - linew / 2.0f;
        auto center = b.center();
        constexpr float twopi = 2.0f * static_cast<float>(M_PI);

        painter.line_width(linew);
        painter.set(Color(dt::kGrayLight, 80));
        painter.draw(Arc(center, radius, 0.0f, twopi));
        painter.stroke();

        constexpr int segments = 90;
        constexpr float arc_span = 5.2f;
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

shared_ptr<Widget> create_wifi_connecting_screen(
    const string& ssid,
    const string& password,
    function<void()> on_success,
    function<void()> on_failure)
{
    // One-shot guard so a late timer tick can't fire navigation twice.
    auto done = make_shared<bool>(false);

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Spinner — centred, slightly above middle.
    const int spin_sz = 200;
    const int spin_x = (dt::SCREEN_W - spin_sz) / 2;
    const int spin_y = 110;
    auto spinner = make_shared<ConnectSpinner>(Rect(spin_x, spin_y, spin_sz, spin_sz));
    container->add(spinner);

    // "Connecting to <SSID>" — under the spinner.
    auto title = make_shared<Label>("Connecting to",
        Rect(0, spin_y + spin_sz + 10, dt::SCREEN_W, 32));
    title->font(Font(dt::FONT_SUBTITLE, Font::Weight::normal));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    title->text_align(AlignFlag::center);
    container->add(title);

    auto ssid_lbl = make_shared<Label>(ssid,
        Rect(0, spin_y + spin_sz + 44, dt::SCREEN_W, 40));
    ssid_lbl->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    ssid_lbl->color(Palette::ColorId::label_text, dt::kGreen);
    ssid_lbl->text_align(AlignFlag::center);
    container->add(ssid_lbl);

    // Status line — updated on failure so the user knows what to do next.
    auto status = make_shared<Label>("",
        Rect(0, spin_y + spin_sz + 92, dt::SCREEN_W, 30));
    status->font(Font(dt::FONT_BODY, Font::Weight::normal));
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    status->text_align(AlignFlag::center);
    container->add(status);

    // Spinner animation.
    auto angle = make_shared<float>(0.0f);
    auto anim = make_shared<PeriodicTimer>(chrono::milliseconds(30));
    anim->on_timeout([spinner, angle]() {
        *angle += 0.10f;
        if (*angle > 2.0f * static_cast<float>(M_PI))
            *angle -= 2.0f * static_cast<float>(M_PI);
        spinner->angle(*angle);
    });
    anim->start();

    // Background connect — WiFiManager::connect() blocks while associating and
    // running DHCP, so it must not run on the UI thread.
    auto result = make_shared<atomic<int>>(-1);  // -1 = running, 0 = fail, 1 = ok
    std::thread([result, ssid, password]() {
        egt_wifi::WiFiManager wifi;
        bool ok = wifi.connect(ssid, password);
        result->store(ok ? 1 : 0);
    }).detach();

    // Poll the result on the UI thread.
    auto poll = make_shared<PeriodicTimer>(chrono::milliseconds(200));
    poll->on_timeout([=]() {
        if (*done) { poll->cancel(); return; }
        int r = result->load();
        if (r == -1) return;  // still connecting

        *done = true;
        poll->cancel();
        anim->cancel();

        if (r == 1) {
            if (on_success) on_success();
        } else {
            // Briefly show the failure reason, then bounce back to the list.
            status->text("Couldn't connect — check the password and try again.");
            status->color(Palette::ColorId::label_text, dt::kRed);
            auto bounce = make_shared<PeriodicTimer>(chrono::milliseconds(1800));
            bounce->on_timeout([=]() {
                bounce->cancel();
                if (on_failure) on_failure();
            });
            bounce->start();
        }
    });
    poll->start();

    return container;
}
