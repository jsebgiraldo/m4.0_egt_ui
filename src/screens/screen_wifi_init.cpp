#include "screen_wifi_init.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include "../wifi/wifi_backend.h"
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>

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

// ── Async WiFi check result (populated by background thread) ────────────────
struct WifiCheckResult {
    bool available = false;
    std::string ssid;
    bool has_saved = false;
};

shared_ptr<Widget> create_wifi_init_screen(
    function<void()> on_connected,
    function<void(shared_ptr<vector<egt_wifi::WiFiNetwork>>)> on_failed,
    function<void()> on_skip)
{
    // One-shot guard: once either callback fires, suppress all subsequent timer
    // callbacks. This prevents background timers from navigating away from whatever
    // screen replaced wifi_init (e.g. after treatment completes).
    auto active = make_shared<bool>(true);
    auto orig_connected = std::move(on_connected);
    auto orig_failed    = std::move(on_failed);
    auto orig_skip      = std::move(on_skip);
    on_connected = [active, orig_connected]() {
        if (!*active) return;
        *active = false;
        if (orig_connected) orig_connected();
    };
    on_failed = [active, orig_failed](shared_ptr<vector<egt_wifi::WiFiNetwork>> nets) {
        if (!*active) return;
        *active = false;
        if (orig_failed) orig_failed(nets);
    };
    on_skip = [active, orig_skip]() {
        if (!*active) return;
        *active = false;
        if (orig_skip) orig_skip();
    };

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Figma: Spinner COMPONENT 214×214 → ~396px scaled, centered vertically
    const int spin_sz = 396;
    const int spin_x = (dt::SCREEN_W - spin_sz) / 2;
    const int spin_y = (dt::SCREEN_H - spin_sz) / 2 - 20;

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

    const bool mock_mode = (std::getenv("EGT_MOCK_WIFI") != nullptr);

    // ── Async WiFi scanning (runs in background thread) ─────────────────────
    auto start_scan = [=]() {
        printf("[WIFI_INIT] no saved networks, scanning...\n");
        fflush(stdout);
        status_label->text("Scanning networks...");

        auto scan_count = make_shared<int>(0);
        auto prev_count = make_shared<int>(-1);
        const int MAX_SCAN = mock_mode ? 2 : 5;

        // Async scan: background thread + result polling
        auto scan_result = make_shared<shared_ptr<vector<egt_wifi::WiFiNetwork>>>(nullptr);
        auto scan_running = make_shared<atomic<bool>>(false);

        auto scan_timer = make_shared<PeriodicTimer>(chrono::milliseconds(500));
        scan_timer->on_timeout([=]() {
            // If a scan is still running, skip this tick (keep UI responsive)
            if (scan_running->load()) return;

            // Check if previous scan delivered results
            if (*scan_result) {
                (*scan_count)++;
                auto nets = *scan_result;
                *scan_result = nullptr;
                int cur = static_cast<int>(nets->size());
                printf("[WIFI_INIT] scan poll %d/%d: %d networks\n", *scan_count, MAX_SCAN, cur);
                fflush(stdout);

                const int MIN_SCAN = mock_mode ? 1 : 3;
                bool stable = (*scan_count >= MIN_SCAN && cur > 0 && cur == *prev_count);
                *prev_count = cur;

                if (stable || *scan_count >= MAX_SCAN) {
                    printf("[WIFI_INIT] scan done, %d networks (stable=%d)\n", cur, stable);
                    fflush(stdout);
                    scan_timer->cancel();
                    anim_timer->cancel();
                    if (on_failed) on_failed(nets);
                    return;
                }
            }

            // Start a new background scan
            scan_running->store(true);
            std::thread([scan_result, scan_running]() {
                egt_wifi::WiFiManager wm;
                auto nets = make_shared<vector<egt_wifi::WiFiNetwork>>(wm.scan_networks());
                *scan_result = nets;
                scan_running->store(false);
            }).detach();
        });
        scan_timer->start();
    };

    // ── Async main polling loop ─────────────────────────────────────────────
    // Background thread performs blocking WiFi checks; timer polls results.
    auto check_result = make_shared<shared_ptr<WifiCheckResult>>(nullptr);
    auto check_running = make_shared<atomic<bool>>(false);
    auto nm_ready      = make_shared<bool>(false);
    auto nm_wait_secs  = make_shared<int>(0);
    auto conn_secs     = make_shared<int>(0);
    // Tighter timeouts so the spinner moves on faster when there's nothing
    // to connect to: NM is usually up within a few seconds, and if there are
    // no saved networks we want to reach the scan UI quickly.
    const int MAX_NM_WAIT   = 6;
    const int MAX_CONN_WAIT = mock_mode ? 2 : 4;

    auto main_timer = make_shared<PeriodicTimer>(chrono::milliseconds(500));
    main_timer->on_timeout([=, start_scan = std::move(start_scan)]() {
        // If background check is still running, skip (keep UI responsive)
        if (check_running->load()) return;

        // Process result from previous background check
        if (*check_result) {
            auto r = *check_result;
            *check_result = nullptr;

            // Phase 1: wait for NetworkManager
            if (!*nm_ready) {
                if (!r->available) {
                    (*nm_wait_secs)++;
                    printf("[WIFI_INIT] waiting for NetworkManager (%ds/%ds)\n",
                        *nm_wait_secs, MAX_NM_WAIT);
                    fflush(stdout);
                    status_label->text("Initializing...");
                    if (*nm_wait_secs >= MAX_NM_WAIT) {
                        printf("[WIFI_INIT] NetworkManager never started\n");
                        fflush(stdout);
                        main_timer->cancel();
                        anim_timer->cancel();
                        if (on_failed) on_failed(nullptr);
                        return;
                    }
                } else {
                    *nm_ready = true;
                    printf("[WIFI_INIT] NetworkManager is ready\n");
                    fflush(stdout);
                }
            }

            // Phase 2: NM up — check connectivity
            if (*nm_ready) {
                if (!r->ssid.empty()) {
                    printf("[WIFI_INIT] connected to '%s'\n", r->ssid.c_str());
                    fflush(stdout);
                    status_label->text("Connected to " + r->ssid);
                    main_timer->cancel();

                    auto done_timer = make_shared<PeriodicTimer>(chrono::milliseconds(1000));
                    done_timer->on_timeout([=]() {
                        done_timer->cancel();
                        anim_timer->cancel();
                        if (on_connected) on_connected();
                    });
                    done_timer->start();
                    return;
                }

                (*conn_secs)++;
                printf("[WIFI_INIT] not connected yet (%ds/%ds)\n", *conn_secs, MAX_CONN_WAIT);
                fflush(stdout);
                status_label->text("Connecting to WiFi...");

                if (*conn_secs >= MAX_CONN_WAIT) {
                    main_timer->cancel();
                    if (!r->has_saved) {
                        start_scan();
                    } else {
                        printf("[WIFI_INIT] timeout with saved networks, showing WiFi list\n");
                        fflush(stdout);
                        anim_timer->cancel();
                        if (on_failed) on_failed(nullptr);
                    }
                    return;
                }
            }
        }

        // Start a new background check (non-blocking)
        check_running->store(true);
        std::thread([check_result, check_running]() {
            egt_wifi::WiFiManager wifi;
            auto r = make_shared<WifiCheckResult>();
            r->available = wifi.is_available();
            if (r->available) {
                r->ssid = wifi.get_current_ssid();
                r->has_saved = wifi.has_saved_networks();
            }
            *check_result = r;
            check_running->store(false);
        }).detach();
    });
    main_timer->start();

    return container;
}
