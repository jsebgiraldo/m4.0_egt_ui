#include <egt/ui>
#include <egt/widget.h>
#include <egt/view.h>
#include <egt/keycode.h>
#include <egt/svgimage.h>

#include "screen_wifi_settings.h"
#include "screen_password_prompt.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"

#include <map>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <tuple>

using namespace egt;
using namespace egt_wifi;
using namespace std;

// ── WiFi signal-arc icon (3 concentric arcs + dot) ──────────────────────────
class WifiIcon : public Widget {
public:
    /// @param signal  0-100 RSSI percentage
    WifiIcon(const Rect& rect, const Color& col, int signal)
        : Widget(rect), m_color(col), m_signal(signal)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        auto center = Point(b.x() + b.width() / 2,
                            b.y() + b.height() - 2);

        constexpr float start = -M_PI * 0.75f;   // -135°
        constexpr float end   = -M_PI * 0.25f;   // -45°

        // How many arcs to light up based on signal strength
        // 0-25  → 0 arcs (dot only)
        // 26-50 → 1 arc
        // 51-75 → 2 arcs
        // 76+   → 3 arcs
        int active = (m_signal > 75) ? 3 : (m_signal > 50) ? 2 : (m_signal > 25) ? 1 : 0;

        Color dim(m_color.red(), m_color.green(), m_color.blue(), 60);

        float radii[] = {7.0f, 13.0f, 19.0f};
        painter.line_width(2.5f);
        for (int i = 0; i < 3; i++) {
            painter.set(i < active ? m_color : dim);
            painter.draw(Arc(center, radii[i], start, end));
            painter.stroke();
        }

        // small dot at the base (always solid)
        painter.set(m_color);
        painter.draw(Arc(center, 2.0f, 0.0f, 2.0f * M_PI));
        painter.fill();
    }

private:
    Color m_color;
    int m_signal;

public:
    void set_color(const Color& c) { m_color = c; damage(); }
    void set_signal(int s) { if (s != m_signal) { m_signal = s; damage(); } }
    int signal() const { return m_signal; }
};

// ── Card gradient: rounded rect filled with the Figma "keyboard gray"
// gradient (244 → 255 top-to-bottom), matching node 151:891. ──────────────
class CardGradient : public Widget {
public:
    CardGradient(const Rect& r, float radius) : Widget(r), m_radius(radius)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& p, const Rect&) override
    {
        auto b = content_area();
        const float x = static_cast<float>(b.x());
        const float y = static_cast<float>(b.y());
        const float w = static_cast<float>(b.width());
        const float h = static_cast<float>(b.height());
        const float r = m_radius;
        const Color top{244, 244, 244};
        const Color bot{255, 255, 255};
        Pattern grad(Pattern::StepArray{{0.0f, top}, {1.0f, bot}},
                     Point(static_cast<int>(x), static_cast<int>(y)),
                     Point(static_cast<int>(x), static_cast<int>(y + h)));
        const auto PI = static_cast<float>(M_PI);
        p.draw(PointF(x + r, y));
        p.line(PointF(x + w - r, y));
        p.draw(Arc(PointF(x + w - r, y + r),     r, -PI / 2, 0.0f));
        p.line(PointF(x + w, y + h - r));
        p.draw(Arc(PointF(x + w - r, y + h - r), r, 0.0f,    PI / 2));
        p.line(PointF(x + r, y + h));
        p.draw(Arc(PointF(x + r, y + h - r),     r, PI / 2,  PI));
        p.line(PointF(x, y + r));
        p.draw(Arc(PointF(x + r, y + r),         r, PI,      3 * PI / 2));
        p.set(grad);
        p.fill();
    }
private:
    float m_radius;
};

// ── Mini scan spinner: small rotating arc shown while a scan is running ────
class MiniSpinner : public Widget {
public:
    explicit MiniSpinner(const Rect& rect) : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
        hide();  // hidden until a scan starts
    }

    void angle(float a) { m_angle = a; if (visible()) damage(); }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        auto dim = static_cast<float>(min(b.width(), b.height()));
        float radius = dim / 2.0f - 3.0f;
        auto center = b.center();
        constexpr float twopi = 2.0f * static_cast<float>(M_PI);

        // Faint track
        painter.line_width(3.0f);
        painter.set(Color(dt::kGrayLight, 90));
        painter.draw(Arc(center, radius, 0.0f, twopi));
        painter.stroke();

        // Bright ~90° arc that rotates
        painter.set(dt::kGreen);
        painter.line_width(3.0f);
        painter.draw(Arc(center, radius, m_angle, m_angle + 1.6f));
        painter.stroke();
    }

private:
    float m_angle{0.0f};
};

// ── Gradient icon disc (#d9d9d9 → #ffffff vertical) ───────────────────────
// Matches Figma's "Ellipse 4" icon backplates (imgEllipse4 / imgGroup126) —
// same disc family used on wifi-not-found. Replaces the previous flat
// kGray200 circles behind the gear and wifi-off bottom buttons.
class GradientDisc : public Widget {
public:
    explicit GradientDisc(const Rect& r) : Widget(r)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& painter, const Rect&) override
    {
        auto b = content_area();
        const float sz = static_cast<float>(min(b.width(), b.height()));
        const float cx = b.x() + b.width()  / 2.0f;
        const float cy = b.y() + b.height() / 2.0f;
        painter.draw(Arc(PointF(cx, cy), sz * 0.50f - 1.0f,
                         0.0f, 2.0f * static_cast<float>(M_PI)));
        Pattern grad(Pattern::StepArray{{0.0f, Color(0xd9, 0xd9, 0xd9)},
                                        {1.0f, dt::kWhite}},
                     Point(b.x(), b.y()),
                     Point(b.x(), b.y() + b.height()));
        painter.set(grad);
        painter.fill();
    }
};

// Figma wifi-off glyph (node 52:2785 "elements", viewBox 27.6269 × 25.3656).
// Verbatim path data from Figma's SVG export — 5 asymmetric stroked arcs
// (fan opens toward the top-right), the small near-pivot ellipse and the
// "\" diagonal slash. "%C" tokens are stroke-colour placeholders. Same art
// as screen_wifi_not_found.cpp.
static const char* kWifiOffGlyphFigmaSvg = R"svg(<svg viewBox="0 0 27.6269 25.3656" xmlns="http://www.w3.org/2000/svg" fill="none">
<path d="M9.32881 15.6035C10.7657 14.3119 12.4864 13.7191 14.4542 13.8759" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M22.1422 12.0987C20.2296 10.5559 18.0224 9.49848 15.7355 9.17814" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M5.48467 12.0985C6.88517 11.0344 8.40357 10.2445 9.96936 9.76196" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M26.6268 8.59373C21.5934 4.71837 16.0381 3.25341 10.6101 4.19888" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M1.00005 8.59387C2.56853 7.38625 4.2034 6.25731 5.48474 5.67317" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<ellipse cx="13.8134" cy="19.6925" rx="1.92201" ry="1.75242" stroke="%C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M1.00011 1.00001L26.6269 24.3656" stroke="%C" stroke-width="2" stroke-linecap="round"/>
</svg>)svg";

// Cache wifi-off SvgImage instances by (RGB, size). Keeps the SvgImage
// alive in a function-static cache so the sliced Image's rasterized buffer
// stays valid on EGT 1.10 (same hazard as load_svg_icon elsewhere).
static Image get_wifi_off_image(const Color& stroke, int size_px)
{
    using Key = std::tuple<uint8_t, uint8_t, uint8_t, int>;
    static std::map<Key, std::shared_ptr<SvgImage>> s_cache;
    Key k{stroke.red(), stroke.green(), stroke.blue(), size_px};
    auto it = s_cache.find(k);
    if (it != s_cache.end()) return static_cast<Image>(*it->second);

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x",
             stroke.red(), stroke.green(), stroke.blue());
    string svg = kWifiOffGlyphFigmaSvg;
    string::size_type pos = 0;
    while ((pos = svg.find("%C", pos)) != string::npos)
        svg.replace(pos, 2, hex);

    char fname[80];
    snprintf(fname, sizeof(fname),
             "/tmp/egt-icon-wifioff-ws-%02x%02x%02x-%d.svg",
             stroke.red(), stroke.green(), stroke.blue(), size_px);
    ofstream f(fname); f << svg; f.close();
    try {
        auto img = std::make_shared<SvgImage>(string("file:") + fname,
                                              SizeF(size_px, size_px));
        s_cache.emplace(k, img);
        return static_cast<Image>(*img);
    } catch (...) {
        return {};
    }
}

// ── Skip WiFi button (wifi-offline glyph) ─────────────────────────────────
// Figma node 151:956 / 154:880: 39×39 gradient disc (#d9d9d9 → #fff, same
// Ellipse-4 family as the other icon discs) with the asymmetric Figma
// wifi-off glyph (node 52:2785) — fan opening toward the top-right, dot near
// the pivot, "\" slash upper-left → lower-right.
class SkipWiFiButton : public Widget {
public:
    explicit SkipWiFiButton(const Rect& rect)
        : Widget(rect)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        const float sz  = static_cast<float>(min(b.width(), b.height()));
        const float cx  = b.x() + b.width()  / 2.0f;
        const float cy  = b.y() + b.height() / 2.0f;

        // Gradient disc backplate (#d9d9d9 top → #ffffff bottom).
        painter.draw(Arc(PointF(cx, cy), sz * 0.50f - 1.0f,
                         0.0f, 2.0f * static_cast<float>(M_PI)));
        Pattern grad(Pattern::StepArray{{0.0f, Color(0xd9, 0xd9, 0xd9)},
                                        {1.0f, dt::kWhite}},
                     Point(b.x(), b.y()),
                     Point(b.x(), b.y() + b.height()));
        painter.set(grad);
        painter.fill();

        // Figma wifi-off SVG glyph at ~65% of the disc, centred — same
        // sizing as screen_wifi_not_found.cpp's make_wifi_off_glyph_frame.
        // Point-then-image idiom: portable across EGT 1.10 and 1.12.
        const int gs = static_cast<int>(sz * 0.65f);
        auto glyph = get_wifi_off_image(dt::kTextPrimary, gs);
        if (!glyph.empty()) {
            painter.draw(PointF(cx - gs / 2.0f, cy - gs / 2.0f));
            painter.draw(glyph);
        }
    }
};

std::shared_ptr<Widget> create_wifi_settings_panel(
    function<void()> on_back,
    function<void()> on_scan_wifi,
    function<void(const string& ssid, const string& password)> on_connect,
    function<void(const egt_wifi::WiFiNetwork&)> on_item_selected,
    function<void(std::shared_ptr<egt::Widget>)> on_show_screen,
    std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> cached_networks)
{
    // ── Scan or use cached networks ─────────────────────────────────────────
    // If no cached networks, do a SHORT synchronous scan with a hard cap so
    // the UI doesn't lock for >3s waiting on nmcli. Caller should ideally
    // pre-scan and pass cached_networks to avoid this path entirely.
    auto nets = cached_networks;
    if (!nets) {
        std::atomic<bool> done{false};
        auto result = std::make_shared<vector<WiFiNetwork>>();
        std::thread([&done, result]() {
            egt_wifi::WiFiManager wifi;
            *result = wifi.scan_networks();
            done.store(true);
        }).detach();
        // Wait up to 3s — past that the user gets an empty list and can
        // hit the "Refresh" button to retry without blocking the screen.
        for (int i = 0; i < 30 && !done.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        nets = result;
    }
    // Sort by signal strength descending. Figma shows the connected AP
    // somewhere in the middle (not pinned to the top), so we don't promote
    // it - the green text + green icons identify it well enough.
    auto net_sort = [](const WiFiNetwork& a, const WiFiNetwork& b) {
        return a.signal > b.signal;
    };
    sort(nets->begin(), nets->end(), net_sort);
    const int total = static_cast<int>(nets->size());

    auto network_map = make_shared<map<string, egt_wifi::WiFiNetwork>>();
    auto alive = make_shared<bool>(true);

    // Per-row widget handles, keyed by SSID. Populated by rebuild_rows(),
    // consulted by update_rows_in_place() so a signal/state change refreshes
    // just the affected widgets instead of recreating every row (no flicker,
    // no scroll churn).
    struct RowHandles {
        std::shared_ptr<Label>    ssid_lbl;
        std::shared_ptr<WifiIcon> wifi_icon;
        std::shared_ptr<Label>    chevron;
        std::shared_ptr<Frame>    row_frame;
    };
    auto row_handles = make_shared<map<string, RowHandles>>();

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kWhite);

    // ── Title - Figma fontSize 14 Bold -> device 26 pt ─────────────────────
    // Figma y=22 → device text bbox y 44–63: rect top at 37 centres the
    // 36-px-tall label band on the Figma optical centre (~54).
    auto title = make_shared<Label>("Establish Wi-Fi Connection",
        Rect(0, 37, dt::SCREEN_W, 36));
    title->font(Font("Gothic A1", 26, Font::Weight::bold));
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    title->text_align(AlignFlag::center);
    container->add(title);

    // ── Determine connected state ───────────────────────────────────────────
    bool any_connected = false;
    for (const auto& net : *nets)
        if (net.connected) { any_connected = true; break; }

    // ── Card container ──────────────────────────────────────────────────────
    // Figma node 151:891: Rectangle 68 at (49, 46), 332×160 figma → device
    // (91, 85, 615, 296). F1:1 with Figma TARGET.
    const int card_x = 91;
    const int card_y = 85;
    const int card_w = 615;
    const int card_h = 296;

    // Card backdrop matches Figma node 151:891's "keyboard gray" gradient
    // (244 → 255 vertical). The Frame itself is transparent; the gradient is
    // painted by a CardGradient child so the rounded corners follow the
    // gradient fill cleanly.
    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({});
    card->border(0);
    container->add(card);
    card->add(make_shared<CardGradient>(Rect(0, 0, card_w, card_h),
                                        static_cast<float>(dt::RADIUS_MD)));

    // "Choose a Network..." header - Figma fontSize 12 Regular -> 22 pt,
    // left-aligned as in the Figma TARGET.
    // Figma x=58 → device x≈107; card_x=91 → card-relative 16.
    auto choose_label = make_shared<Label>("Choose a Network...",
        Rect(16, 15, 300, 28));
    choose_label->font(Font("Gothic A1", 22, Font::Weight::normal));
    choose_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    choose_label->text_align(AlignFlag::left | AlignFlag::center_vertical);
    card->add(choose_label);

    // Scan spinner — sits in the top-right corner of the card, well clear of
    // the "Choose a Network..." text. Shown only while a background scan is in
    // flight so the user knows the list is live. Hidden by default so the
    // static header looks clean.
    auto scan_spinner = make_shared<MiniSpinner>(Rect(card_w - 40, 18, 22, 22));
    card->add(scan_spinner);
    scan_spinner->hide();

    auto spinner_anim = make_shared<PeriodicTimer>(chrono::milliseconds(40));
    auto spinner_angle = make_shared<float>(0.0f);
    spinner_anim->on_timeout([scan_spinner, spinner_angle]() {
        *spinner_angle += 0.22f;
        if (*spinner_angle > 2.0f * static_cast<float>(M_PI))
            *spinner_angle -= 2.0f * static_cast<float>(M_PI);
        scan_spinner->angle(*spinner_angle);
    });

    // Header separator line
    auto hdr_line = make_shared<Frame>(Rect(10, 48, card_w - 20, 1));
    hdr_line->fill_flags({Theme::FillFlag::blend});
    hdr_line->color(Palette::ColorId::bg, dt::kGrayLight);
    hdr_line->border(0);
    card->add(hdr_line);

    // ── Scrollable network list ─────────────────────────────────────────────
    // Row pitch sampled from Figma: BTWiFi at y=561, BTWiFi-With-Fon at
    // y=591 -> 30 figma px between rows -> 56 device px.
    const int list_top = 52;
    const int list_h = card_h - list_top;
    // Figma row pitch is 30 figma px = 56 device. Rows have no gap; the
    // visual separation comes from a thin divider line drawn at the bottom
    // edge of each row (below).
    const int row_h = 56;
    const int row_gap = 0;
    const int row_pitch = row_h + row_gap;     // 56 = figma 30 * SCALE
    // SSID left edge: figma x=68 → device x≈126; card_x=91 → card-relative
    // 35. Rows are indented 10 figma px PAST the "Choose a Network..." header.
    const int text_pad = 35;

    // Total rows: networks + "Other..." entry
    const int total_rows = total + 1;
    const int content_h = total_rows > 0 ? (total_rows * row_pitch - row_gap) : 0;

    // ScrolledView in EGT 1.10 (target build) was failing to render its child
    // Frame, so we use a plain clipping Frame and implement scroll-by-drag
    // manually. Rows that fall outside the visible band are clipped by EGT's
    // default child clipping; touch-drag on the scroll_view moves the inner
    // list_content's y to bring hidden rows into view.
    auto scroll_view = make_shared<Frame>(
        Rect(0, list_top, card_w, list_h));
    scroll_view->fill_flags({});
    scroll_view->border(0);
    // EGT requires user_drag(true) for a widget to accept pointer_drag*
    // events at all (Widget::accept_drag()), and user_track_drag(true) so the
    // drag keeps firing once the pointer crosses outside the widget bounds.
    // Without these flags the drag silently bails after the first event.
    scroll_view->user_drag(true);
    scroll_view->user_track_drag(true);
    card->add(scroll_view);

    // Content frame inside the clipping view - holds all rows.
    auto list_content = make_shared<Frame>(
        Rect(0, 0, card_w, content_h));
    list_content->fill_flags({Theme::FillFlag::blend});
    list_content->color(Palette::ColorId::bg, dt::kTransparent);
    list_content->border(0);
    list_content->user_drag(true);
    list_content->user_track_drag(true);
    scroll_view->add(list_content);

    // Touch-drag scrolling: track pointer delta and shift list_content's y.
    // Clamped to [-(content_h - list_h), 0] so we never reveal empty space.
    auto drag_origin_y = make_shared<int>(0);
    auto drag_start_y  = make_shared<int>(0);
    auto drag_active   = make_shared<bool>(false);

    // Shared drag-scroll helper. EGT does not auto-propagate pointer events
    // from a child to its ancestor's on_event callbacks, so a drag that
    // STARTS on a row never reaches scroll_view's handler — every row has to
    // forward the same drag event into here for the list to actually scroll.
    // Incremental delta tracking instead of "origin + cumulative dy" because
    // the row that captured the press can be destroyed mid-gesture by the
    // periodic rebuild_rows() pass on a mock-data tick — in that case EGT
    // delivers pointer_drag events to the NEW row without re-firing the
    // drag_start. Storing only the previous y and computing delta per event
    // is robust to that.
    auto last_y = make_shared<int>(0);
    auto handle_scroll_drag = [=](Event& e) {
        if (e.id() == EventId::pointer_drag_start) {
            *last_y      = e.pointer().point.y();
            *drag_active = true;
        } else if (e.id() == EventId::pointer_drag) {
            if (!*drag_active) {
                // Missed drag_start (likely a mid-gesture row rebuild). Use
                // this event as the new anchor and start scrolling on the
                // next one.
                *last_y      = e.pointer().point.y();
                *drag_active = true;
                return;
            }
            const int cur_y = e.pointer().point.y();
            const int dy    = cur_y - *last_y;
            int new_y = list_content->y() + dy;
            const int min_y = -(list_content->height() - list_h);
            if (min_y > 0) new_y = 0;          // content fits, no scroll
            else if (new_y > 0) new_y = 0;
            else if (new_y < min_y) new_y = min_y;
            list_content->move(Point(0, new_y));
            *last_y = cur_y;
        } else if (e.id() == EventId::pointer_drag_stop) {
            *drag_active = false;
        }
    };
    (void)drag_origin_y; (void)drag_start_y;   // kept above for ABI parity
    scroll_view->on_event([=](Event& e) { handle_scroll_drag(e); });

    // ── Network list rows (rebuilt on each WiFi scan update) ────────────────
    // Wrapping the row construction in a lambda lets the periodic refresh timer
    // swap the row set in-place when nmcli reports new APs, without recreating
    // the whole screen (which would lose scroll position and cause flicker).
    auto rebuild_rows = [=]() {
        // Save scroll position before wiping rows so the user's view doesn't
        // jump back to index 0 on every refresh.
        const int saved_voffset = /* scroll_view->voffset() */ 0;

        list_content->remove_all();

        const int cur_total = static_cast<int>(nets->size());
        const int cur_rows  = cur_total + 1;  // +1 for "Other..."
        const int cur_content_h = cur_rows > 0 ? (cur_rows * row_pitch - row_gap) : 0;
        list_content->resize(Size(card_w, cur_content_h));

        network_map->clear();
        row_handles->clear();

        for (int i = 0; i < cur_total; i++) {
            const auto& net = (*nets)[i];
            int row_y = i * row_pitch;

            string label = net.ssid;
            (*network_map)[label] = net;

            Color text_color = net.connected ? dt::kGreen : dt::kTextPrimary;

            // Row bg is transparent so the card's vertical gradient shows
            // through. Separation comes from the thin divider line drawn at
            // the bottom edge of each row.
            auto row_frame = make_shared<Frame>(
                Rect(0, row_y, card_w, row_h));
            row_frame->fill_flags({});
            row_frame->border(0);
            // Required for EGT to deliver pointer_drag* to this widget — see
            // the user_drag note on scroll_view above.
            row_frame->user_drag(true);
            row_frame->user_track_drag(true);
            list_content->add(row_frame);

            // Row name - Figma fontSize 12 Bold -> device 22 pt.
            auto ssid_lbl = make_shared<Label>(label,
                Rect(text_pad, 4, card_w - 180, row_h - 8));
            ssid_lbl->font(Font("Gothic A1", 22, Font::Weight::bold));
            ssid_lbl->color(Palette::ColorId::label_text, text_color);
            ssid_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
            ssid_lbl->border(0);
            row_frame->add(ssid_lbl);

            // WiFi signal arcs from Figma PNG. Connected row uses the green
            // variant (node 151:922); other rows use the gray variant
            // (node 151:918). Both are 15x10 figma px -> device 28x19 natural,
            // scaled to 50x33 for visual parity with figma.
            std::shared_ptr<WifiIcon> wifi_icon;   // kept as fallback handle
            const std::string signal_png = ui::asset_path(net.connected
                ? "wifi-row-signal-green" : "wifi-row-signal");
            try {
                auto img = Image(("file:" + signal_png).c_str());
                auto wifi_lbl = make_shared<ImageLabel>(img);
                wifi_lbl->autoresize(false);
                wifi_lbl->border(0); wifi_lbl->padding(0); wifi_lbl->margin(0);
                wifi_lbl->fill_flags({});
                wifi_lbl->image_align(AlignFlag::center);
                wifi_lbl->box(Rect(card_w - 130, (row_h - 33) / 2, 50, 33));
                row_frame->add(wifi_lbl);
            } catch (...) {
                wifi_icon = make_shared<WifiIcon>(
                    Rect(card_w - 130, (row_h - 33) / 2, 50, 33),
                    text_color, net.signal);
                row_frame->add(wifi_icon);
            }

            // Chevron right - PNG from Figma. Connected row uses the green
            // variant (node 151:907), other rows the gray (node 151:904).
            const std::string chev_path = ui::asset_path(net.connected
                ? "wifi-row-chevron-green" : "wifi-row-chevron");
            std::shared_ptr<Label> chevron;
            try {
                auto chev_img = Image(("file:" + chev_path).c_str());
                auto chev_lbl = make_shared<ImageLabel>(chev_img);
                chev_lbl->autoresize(false);
                chev_lbl->border(0); chev_lbl->padding(0); chev_lbl->margin(0);
                chev_lbl->fill_flags({});
                chev_lbl->image_align(AlignFlag::center);
                chev_lbl->box(Rect(card_w - 64, (row_h - 46) / 2, 46, 46));
                row_frame->add(chev_lbl);
            } catch (...) { /* fall back: no chevron */ }
            // Keep a transparent Label as the chevron handle for the
            // colour-update logic below (no-op when no real chevron drawn).
            chevron = make_shared<Label>("",
                Rect(card_w - 64, (row_h - 46) / 2, 46, 46));
            chevron->color(Palette::ColorId::label_text, text_color);
            row_frame->add(chevron);

            auto hover_timer = make_shared<Timer>(chrono::milliseconds(1500));
            hover_timer->on_timeout([=]() {
                ssid_lbl->color(Palette::ColorId::label_text, text_color);
                if (wifi_icon) wifi_icon->set_color(text_color);
                chevron->color(Palette::ColorId::label_text, text_color);
                row_frame->damage();
            });
            row_frame->on_event([=](Event& event) {
                if (event.id() == EventId::raw_pointer_down) {
                    hover_timer->stop();
                    ssid_lbl->color(Palette::ColorId::label_text, dt::kGreen);
                    if (wifi_icon) wifi_icon->set_color(dt::kGreen);
                    chevron->color(Palette::ColorId::label_text, dt::kGreen);
                    row_frame->damage();
                } else if (event.id() == EventId::raw_pointer_up) {
                    hover_timer->start();
                } else if (event.id() == EventId::pointer_drag_start ||
                           event.id() == EventId::pointer_drag ||
                           event.id() == EventId::pointer_drag_stop) {
                    hover_timer->stop();
                    ssid_lbl->color(Palette::ColorId::label_text, text_color);
                    if (wifi_icon) wifi_icon->set_color(text_color);
                    chevron->color(Palette::ColorId::label_text, text_color);
                    row_frame->damage();
                    // Forward to the scroll-view's drag logic so dragging on a
                    // row actually scrolls the list (EGT does not bubble).
                    handle_scroll_drag(event);
                }
            });

            row_frame->on_event([=](Event&) {
                *alive = false;
                auto selected_net = (*network_map)[label];
                auto pwd_screen = create_password_prompt_screen(
                    "Network: " + selected_net.ssid,
                    "Enter password to join",
                    "Join", "Back",
                    [=](const string& password) {
                        on_connect(selected_net.ssid, password);
                    },
                    [=]() {
                        if (on_show_screen) {
                            on_show_screen(create_wifi_settings_panel(
                                on_back, on_scan_wifi, on_connect,
                                on_item_selected, on_show_screen, nets));
                        }
                    });
                if (on_show_screen) on_show_screen(pwd_screen);
            }, {EventId::pointer_click});

            // Row divider line - matches figma's visible row separator.
            auto sep = make_shared<Frame>(
                Rect(15, row_h - 1, card_w - 30, 1));
            sep->fill_flags({Theme::FillFlag::blend});
            sep->color(Palette::ColorId::bg, Color(220, 220, 220));
            sep->border(0);
            row_frame->add(sep);

            // Stash widget handles so update_rows_in_place() can refresh this
            // row's signal/state without a full rebuild.
            (*row_handles)[label] = RowHandles{ssid_lbl, wifi_icon, chevron, row_frame};
        }

        // "Other..." entry at the bottom — always present, transparent bg.
        int other_y = cur_total * row_pitch;
        auto other_frame = make_shared<Frame>(
            Rect(0, other_y, card_w, row_h));
        other_frame->fill_flags({});
        other_frame->border(0);
        other_frame->user_drag(true);
        other_frame->user_track_drag(true);
        list_content->add(other_frame);

        auto other_lbl = make_shared<Label>("Other...",
            Rect(text_pad, 4, card_w - 180, row_h - 8));
        other_lbl->font(Font("Gothic A1", 22, Font::Weight::bold));
        other_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        other_lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
        other_lbl->border(0);
        other_frame->add(other_lbl);

        other_frame->on_event([=](Event&) {
            *alive = false;
            auto ssid_screen = create_password_prompt_screen(
                "Other Network",
                "Enter the network name (SSID)",
                "Next", "Back",
                [=](const string& ssid) {
                    auto pwd_screen = create_password_prompt_screen(
                        "Network: " + ssid,
                        "Enter password to join",
                        "Join", "Back",
                        [=](const string& password) {
                            on_connect(ssid, password);
                        },
                        [=]() {
                            if (on_show_screen) {
                                on_show_screen(create_wifi_settings_panel(
                                    on_back, on_scan_wifi, on_connect,
                                    on_item_selected, on_show_screen));
                            }
                        });
                    if (on_show_screen) on_show_screen(pwd_screen);
                },
                [=]() {
                    if (on_show_screen) {
                        on_show_screen(create_wifi_settings_panel(
                            on_back, on_scan_wifi, on_connect,
                            on_item_selected, on_show_screen));
                    }
                });
            if (on_show_screen) on_show_screen(ssid_screen);
        }, {EventId::pointer_click});

        // Same scroll-forwarding as the other rows so dragging starting on
        // "Other..." actually scrolls the list (EGT does not bubble).
        other_frame->on_event([=](Event& event) {
            if (event.id() == EventId::pointer_drag_start ||
                event.id() == EventId::pointer_drag ||
                event.id() == EventId::pointer_drag_stop) {
                handle_scroll_drag(event);
            }
        });

        // "Other..." wifi signal icon — Figma node 151:930 puts the gray
        // signal-arc glyph on this row too, same box as the network rows.
        try {
            auto sig_img = Image(
                ("file:" + ui::asset_path("wifi-row-signal")).c_str());
            auto sig_lbl = make_shared<ImageLabel>(sig_img);
            sig_lbl->autoresize(false);
            sig_lbl->border(0); sig_lbl->padding(0); sig_lbl->margin(0);
            sig_lbl->fill_flags({});
            sig_lbl->image_align(AlignFlag::center);
            sig_lbl->box(Rect(card_w - 130, (row_h - 33) / 2, 50, 33));
            other_frame->add(sig_lbl);
        } catch (...) { /* fall back: no icon */ }

        // "Other..." chevron - same PNG as the network rows for consistency.
        try {
            auto img = Image(("file:" + ui::asset_path("wifi-row-chevron")).c_str());
            auto chev_lbl = make_shared<ImageLabel>(img);
            chev_lbl->autoresize(false);
            chev_lbl->border(0); chev_lbl->padding(0); chev_lbl->margin(0);
            chev_lbl->fill_flags({});
            chev_lbl->image_align(AlignFlag::center);
            chev_lbl->box(Rect(card_w - 64, (row_h - 46) / 2, 46, 46));
            other_frame->add(chev_lbl);
        } catch (...) { /* fall back: no chevron */ }

        // Separator line under "Other..." — Figma line 151:903 (figma y=200
        // → device y≈367-370), same style as the network-row dividers.
        auto other_sep = make_shared<Frame>(
            Rect(15, row_h - 1, card_w - 30, 1));
        other_sep->fill_flags({Theme::FillFlag::blend});
        other_sep->color(Palette::ColorId::bg, Color(220, 220, 220));
        other_sep->border(0);
        other_frame->add(other_sep);

        list_content->damage();

        // Scroll position restore is no-op while scroll_view is a plain Frame.
        (void)saved_voffset; (void)cur_content_h;
    };

    // Cheap refresh path: SSID set unchanged, only signal/connected differs.
    // Updates the existing widgets (icon strength, text colour) and slides
    // rows to their new sorted position via move() — no remove/re-add, so
    // there is zero flicker and the scroll position is untouched.
    auto update_rows_in_place = [=]() {
        for (int i = 0; i < static_cast<int>(nets->size()); ++i) {
            const auto& net = (*nets)[i];
            auto it = row_handles->find(net.ssid);
            if (it == row_handles->end()) continue;  // shouldn't happen
            auto& h = it->second;

            // Refresh tap-target data so a tap uses current signal/state.
            (*network_map)[net.ssid] = net;

            // Signal strength arcs.
            if (h.wifi_icon) h.wifi_icon->set_signal(net.signal);

            // Connected → green; otherwise default text colour.
            Color text_color = net.connected ? dt::kGreen : dt::kTextPrimary;
            if (h.ssid_lbl)  h.ssid_lbl->color(Palette::ColorId::label_text, text_color);
            if (h.wifi_icon) h.wifi_icon->set_color(text_color);
            if (h.chevron)   h.chevron->color(Palette::ColorId::label_text, text_color);

            // Slide the row to its new sorted slot (connected-first ordering
            // means a freshly-connected AP rises to the top).
            const int target_y = i * row_pitch;
            if (h.row_frame && h.row_frame->y() != target_y) {
                h.row_frame->move(Point(h.row_frame->x(), target_y));
            }
            if (h.row_frame) h.row_frame->damage();
        }
        list_content->damage();
        printf("[WIFI_SETTINGS_DBG] rebuild done: list_content kids=%zu visible=%d\n",
               list_content->count_children(), list_content->visible());
        fflush(stdout);
    };

    rebuild_rows();

    // ── Bottom icons - F1:1 with Figma ──────────────────────────────────────
    // Both icons are 39×39 figma → 72×72 device, matching the same gray
    // gradient pill style on Figma (217→255). Y position = figma 206 →
    // device 381. Gear (Group 263 inner pill at figma x=18) sits at device
    // x=33; wifi-off (Group 248) sits at figma x=375 → device x=695.
    const int icon_sz = 72;
    const int icon_y  = 381;

    // ── Gear button (bottom-left) - Figma node 2065:870, inner pill at
    // (7,7) 39×39 inside the 133×52 group. The pill takes the user back to
    // wherever they came from. home-gear-icon.png already bakes in the
    // gradient circle background + gear glyph, so we render it as the whole
    // button (no extra pill behind it).
    {
        auto gear_wrap = make_shared<Frame>(Rect(33, icon_y, icon_sz, icon_sz));
        gear_wrap->fill_flags({});
        container->add(gear_wrap);

        // Gradient disc backplate (#d9d9d9 → #fff vertical) — Figma
        // Ellipse 4 (imgEllipse4), same disc family as the other icon
        // circles. Crisp circle edge, gradient fill, glyph overlaid on top.
        gear_wrap->add(make_shared<GradientDisc>(Rect(0, 0, icon_sz, icon_sz)));

        try {
            // wifi-settings-gear.png is the bare gear glyph (no background),
            // sized 57×57 source. Figma gear bbox ≈ 51×51 device on the
            // 72-px disc (glyph/disc ≈ 0.70) → render at 52 device px.
            const int glyph_sz = 52;
            auto img = Image(("file:" + ui::asset_path("wifi-settings-gear")).c_str(),
                             static_cast<float>(glyph_sz) / 57.0f,
                             static_cast<float>(glyph_sz) / 57.0f);
            auto gear_lbl = make_shared<ImageLabel>(img);
            gear_lbl->autoresize(false);
            gear_lbl->border(0); gear_lbl->padding(0); gear_lbl->margin(0);
            gear_lbl->fill_flags({});
            gear_lbl->image_align(AlignFlag::center);
            gear_lbl->box(Rect((icon_sz - glyph_sz) / 2,
                               (icon_sz - glyph_sz) / 2,
                               glyph_sz, glyph_sz));
            gear_wrap->add(gear_lbl);
        } catch (...) { /* circle alone is enough as a tap target */ }

        gear_wrap->on_event([alive, on_back](Event& e) {
            if (e.id() == EventId::pointer_click) {
                *alive = false;
                if (on_back) on_back();
            }
        });
    }

    // ── Wi-Fi-off icon (bottom-right) - Figma node 151:956 (Group 248) at
    // figma (375, 206), 39×39 → device (695, 381), 72×72. Tapping goes to
    // the "Not Connected" screen via on_connect("", "").
    {
        const int icon_x = 695;
        auto skip_btn = make_shared<SkipWiFiButton>(
            Rect(icon_x, icon_y, icon_sz, icon_sz));
        container->add(skip_btn);

        skip_btn->on_event([=](Event&) {
            *alive = false;
            on_connect("", "");
        }, {EventId::pointer_click});
    }

    // ── Auto-retry scan when no networks found ──────────────────────────────
    if (nets->empty() && on_show_screen) {
        // Place the label just below the "Other..." entry. When nets is empty
        // the "Other..." entry sits at y=0, so this places "Scanning..." right
        // below it.
        const int scan_label_y =
            static_cast<int>(nets->size()) * row_pitch + row_h + 5;
        auto scan_label = make_shared<Label>("Scanning for networks...",
            Rect(30, scan_label_y, 400, 28));
        scan_label->font(Font(16, Font::Weight::normal));
        scan_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
        list_content->add(scan_label);

        auto retry_count = make_shared<int>(0);
        auto scan_result = make_shared<shared_ptr<vector<WiFiNetwork>>>(nullptr);
        auto scan_running = make_shared<atomic<bool>>(false);

        auto retry_timer = make_shared<PeriodicTimer>(chrono::milliseconds(1000));
        retry_timer->on_timeout([=]() {
            if (!*alive) { retry_timer->cancel(); return; }

            // If scan running, skip (keep UI responsive)
            if (scan_running->load()) return;

            // Check result from previous scan
            if (*scan_result) {
                auto fresh = *scan_result;
                *scan_result = nullptr;
                if (!fresh->empty()) {
                    retry_timer->cancel();
                    *alive = false;
                    on_show_screen(create_wifi_settings_panel(
                        on_back, on_scan_wifi, on_connect,
                        on_item_selected, on_show_screen, fresh));
                    return;
                }
            }

            (*retry_count)++;
            printf("[WIFI_SETTINGS] auto-retry scan %d/10\n", *retry_count);
            fflush(stdout);
            if (*retry_count > 10) {
                retry_timer->cancel();
                scan_label->text("No networks found");
                return;
            }

            // Start background scan
            scan_running->store(true);
            std::thread([scan_result, scan_running]() {
                WiFiManager wifi;
                auto fresh = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());
                *scan_result = fresh;
                scan_running->store(false);
            }).detach();
        });
        retry_timer->start();
    }

    // ── Periodic refresh of the network list (every 2 s, in-place) ──────────
    // Polls nmcli on a background thread and only swaps the row set when the
    // AP list actually changed (SSID set, connection state, or signal bucket).
    // Refreshes by calling rebuild_rows() — no screen recreation, preserves
    // scroll position and avoids flicker. Robust to transient nmcli failures:
    // if the scan returns empty while we previously had APs, we keep the last
    // known good list rather than clearing the UI.
    {
        auto refresh_result = make_shared<shared_ptr<vector<WiFiNetwork>>>(nullptr);
        auto refresh_running = make_shared<atomic<bool>>(false);
        auto last_fingerprint = make_shared<string>();

        auto fingerprint = [](const vector<WiFiNetwork>& v) {
            vector<string> parts;
            parts.reserve(v.size());
            for (const auto& n : v) {
                // Signal bucketed to 25-point steps. Real WiFi RSSI fluctuates
                // ±5 dBm easily; tighter buckets caused a rebuild every refresh
                // tick and reset the user's scroll position. 25 still catches a
                // genuine signal drop (e.g. 80→50) without churning on noise.
                int bucket = n.signal / 25;
                parts.push_back(n.ssid + ":" +
                                (n.connected ? "1" : "0") + ":" +
                                to_string(bucket));
            }
            sort(parts.begin(), parts.end());
            string fp;
            for (const auto& p : parts) fp += p + ",";
            return fp;
        };
        *last_fingerprint = fingerprint(*nets);

        auto refresh_timer = make_shared<PeriodicTimer>(chrono::seconds(2));
        refresh_timer->on_timeout([=]() {
            if (!*alive) {
                refresh_timer->cancel();
                spinner_anim->cancel();
                return;
            }

            // Scan still in flight — keep the spinner spinning, come back later.
            if (refresh_running->load()) return;

            // Past the guard → previous scan finished. Stop the spinner.
            if (scan_spinner->visible()) {
                scan_spinner->hide();
                spinner_anim->cancel();
            }

            if (*refresh_result) {
                auto fresh = *refresh_result;
                *refresh_result = nullptr;

                // Guard against transient nmcli failures (subprocess error,
                // I/O error on the rootfs, NM restart, etc). Don't wipe the
                // list if we previously had APs — keep last known good.
                if (fresh->empty() && !nets->empty()) {
                    printf("[WIFI_SETTINGS] empty scan, keeping last list\n");
                    fflush(stdout);
                } else {
                    auto fp = fingerprint(*fresh);
                    if (fp != *last_fingerprint) {
                        sort(fresh->begin(), fresh->end(), net_sort);
                        // Build the SSID-set signature (ignoring signal) to
                        // decide between a cheap in-place refresh and a full
                        // row rebuild. Full rebuild only when APs appear or
                        // disappear — signal/state changes update in place.
                        auto ssid_set = [](const vector<WiFiNetwork>& v) {
                            vector<string> s;
                            s.reserve(v.size());
                            for (auto& n : v) s.push_back(n.ssid);
                            sort(s.begin(), s.end());
                            string out;
                            for (auto& x : s) out += x + "\n";
                            return out;
                        };
                        bool set_changed = ssid_set(*nets) != ssid_set(*fresh);
                        // Defer the destructive rebuild while the user is
                        // mid-drag — destroying their currently-pressed row
                        // breaks the gesture. Leave nets / last_fingerprint
                        // untouched so the next scan tick (2 s) re-tries the
                        // comparison and rebuilds once the drag has ended.
                        if (set_changed && *drag_active) {
                            printf("[WIFI_SETTINGS] rebuild deferred (drag in progress)\n");
                            fflush(stdout);
                        } else {
                        *nets = *fresh;
                        *last_fingerprint = fp;
                        if (set_changed) {
                            printf("[WIFI_SETTINGS] AP set changed — full rebuild (%zu APs)\n",
                                   nets->size());
                            fflush(stdout);
                            rebuild_rows();
                        } else {
                            printf("[WIFI_SETTINGS] signal/state update in place (%zu APs)\n",
                                   nets->size());
                            fflush(stdout);
                            update_rows_in_place();
                        }
                    }
                    }
                }
            }

            refresh_running->store(true);
            scan_spinner->show();
            spinner_anim->start();
            std::thread([refresh_result, refresh_running]() {
                WiFiManager wifi;
                auto fresh = make_shared<vector<WiFiNetwork>>(wifi.scan_networks());
                *refresh_result = fresh;
                refresh_running->store(false);
            }).detach();
        });
        refresh_timer->start();
    }

    return container;
}