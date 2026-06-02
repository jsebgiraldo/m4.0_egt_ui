#include "components.h"
#include "design_tokens.h"
#include "palette.h"
#include "../generated/embedded_assets.h"
#include <cmath>
#include <fstream>

using namespace egt;
using namespace std;

namespace ui {

// ── ShadowedCard ────────────────────────────────────────────────────────────
namespace {
void draw_rounded_path(Painter& p, float x, float y, float w, float h, float r)
{
    const float PI = static_cast<float>(M_PI);
    p.draw(PointF(x + r,         y));
    p.line(PointF(x + w - r,     y));
    p.draw(Arc(PointF(x + w - r, y + r),       r, -PI / 2, 0.0f));
    p.line(PointF(x + w,         y + h - r));
    p.draw(Arc(PointF(x + w - r, y + h - r),   r, 0.0f,    PI / 2));
    p.line(PointF(x + r,         y + h));
    p.draw(Arc(PointF(x + r,     y + h - r),   r, PI / 2,  PI));
    p.line(PointF(x,             y + r));
    p.draw(Arc(PointF(x + r,     y + r),       r, PI,    3 * PI / 2));
}
} // namespace

ShadowedCard::ShadowedCard(const Rect& card_rect,
                           float corner_radius,
                           std::function<void()> on_click)
    : Widget(Rect(card_rect.x() - SHADOW_PAD,
                  card_rect.y() - SHADOW_PAD,
                  card_rect.width()  + 2 * SHADOW_PAD,
                  card_rect.height() + 2 * SHADOW_PAD))
    , m_radius(corner_radius)
    , m_card_rect(card_rect)
    , m_on_click(std::move(on_click))
{
    fill_flags({Theme::FillFlag::blend});
    border(0);
    if (m_on_click) {
        on_event([this](Event& e) {
            if (e.id() == EventId::pointer_click) m_on_click();
        });
    }
}

void ShadowedCard::draw(Painter& painter, const Rect& /*clip*/)
{
    const float r = m_radius;
    const float x = static_cast<float>(m_card_rect.x());
    const float y = static_cast<float>(m_card_rect.y());
    const float w = static_cast<float>(m_card_rect.width());
    const float h = static_cast<float>(m_card_rect.height());

    // Figma DROP_SHADOW offset (0,0), radius 10, rgba(0,0,0,0.10):
    // 16 concentric rounded rects, 0.5 px grow each, alpha 2 per layer.
    // Cumulative inner-edge alpha ~30/255 (12 %), tapering to 2/255.
    constexpr int     shadow_steps    = 16;
    constexpr float   shadow_extent   = 8.0f;
    constexpr uint8_t per_layer_alpha = 2;
    for (int i = shadow_steps; i >= 1; --i)
    {
        float grow = static_cast<float>(i) * (shadow_extent / shadow_steps);
        draw_rounded_path(painter, x - grow, y - grow,
                          w + 2.0f * grow, h + 2.0f * grow,
                          r + grow * 0.5f);
        painter.set(Color(0, 0, 0, per_layer_alpha));
        painter.fill();
    }

    // White card on top.
    draw_rounded_path(painter, x, y, w, h, r);
    painter.set(dt::kWhite);
    painter.fill();
}

// Write embedded PNG to temp file once, return path
static string get_logo_path()
{
    static const string path = "/tmp/egt-logo.png";
    static bool written = false;
    if (!written)
    {
        ofstream f(path, ios::binary);
        f.write(reinterpret_cast<const char*>(assets_figma_images_Lice_logo_png),
                assets_figma_images_Lice_logo_png_len);
        written = f.good();
        printf("[LOGO] wrote %u bytes to %s: %s\n",
               assets_figma_images_Lice_logo_png_len, path.c_str(),
               written ? "OK" : "FAIL");
        fflush(stdout);
    }
    return path;
}

// ── Logo ────────────────────────────────────────────────────────────────────
shared_ptr<Widget> create_logo(int x, int y, int w, int h)
{
    try
    {
        auto path = get_logo_path();
        float hscale = static_cast<float>(w) / 315.0f;
        float vscale = static_cast<float>(h) / 197.0f;
        float s = min(hscale, vscale);
        auto img = Image("file:" + path, s, s);
        printf("[LOGO] file image: %dx%d (scale %.3f)\n", img.width(), img.height(), s);
        fflush(stdout);
        auto logo = make_shared<ImageLabel>(img);
        logo->fill_flags({Theme::FillFlag::blend});
        logo->image_align(AlignFlag::center);
        logo->move(Point(x, y));
        logo->resize(Size(w, h));
        return logo;
    }
    catch (const std::exception& e)
    {
        printf("[LOGO] EXCEPTION: %s\n", e.what());
        fflush(stdout);
        auto ph = make_shared<Frame>(Rect(x, y, w, h));
        ph->fill_flags({Theme::FillFlag::blend});
        ph->color(Palette::ColorId::bg, dt::kGrayBg);
        ph->border(1);
        ph->color(Palette::ColorId::border, dt::kGrayLight);

        auto lbl = make_shared<Label>("LOGO", Rect(0, 0, w, h));
        lbl->font(Font(10));
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        ph->add(lbl);
        return ph;
    }
}

// ── Header Bar ──────────────────────────────────────────────────────────────
shared_ptr<Frame> create_header_bar(
    const string& title,
    bool demo_mode)
{
    auto bar = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::HEADER_H));
    bar->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo (top-left, small)
    auto logo = create_logo(10, 5, 80, 50);
    bar->add(logo);

    // Title (center)
    if (!title.empty()) {
        auto lbl = make_shared<Label>(title,
            Rect(100, 0, dt::SCREEN_W - 200, dt::HEADER_H));
        lbl->font(dt::fontSubtitle());
        lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        bar->add(lbl);
    }

    // Demo mode badge text (top-right area)
    if (demo_mode) {
        auto badge = make_shared<Label>("DEMO MODE",
            Rect(dt::SCREEN_W - 200, 15, 180, 30), AlignFlag::right);
        badge->font(Font(16, Font::Weight::bold));
        badge->color(Palette::ColorId::label_text, dt::kAccentCyan);
        bar->add(badge);
    }

    return bar;
}

// ── Buttons ─────────────────────────────────────────────────────────────────
shared_ptr<Button> create_outlined_button(
    const string& text,
    const Rect& rect,
    function<void()> on_click)
{
    auto btn = make_shared<Button>(text, rect);
    btn->font(dt::fontButton());
    btn->color(Palette::ColorId::button_bg, dt::kWhite);
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->color(Palette::ColorId::border, dt::kGrayLight);
    btn->border(2);
    btn->border_radius(dt::RADIUS_MD);
    if (on_click) {
        btn->on_click([on_click](Event&) { on_click(); });
    }
    return btn;
}

shared_ptr<Button> create_filled_button(
    const string& text,
    const Rect& rect,
    function<void()> on_click)
{
    auto btn = make_shared<Button>(text, rect);
    btn->font(dt::fontButton());
    btn->color(Palette::ColorId::button_bg, dt::kAccentCyan);
    btn->color(Palette::ColorId::button_text, dt::kWhite);
    btn->border(0);
    btn->border_radius(dt::RADIUS_MD);
    if (on_click) {
        btn->on_click([on_click](Event&) { on_click(); });
    }
    return btn;
}

shared_ptr<Button> create_text_button(
    const string& text,
    const Rect& rect,
    function<void()> on_click)
{
    auto btn = make_shared<Button>(text, rect);
    btn->font(dt::fontSmall());
    btn->color(Palette::ColorId::button_bg, dt::kTransparent);
    btn->color(Palette::ColorId::button_text, dt::kTextPrimary);
    btn->border(0);
    if (on_click) {
        btn->on_click([on_click](Event&) { on_click(); });
    }
    return btn;
}

// ── Segmented Progress Bar ──────────────────────────────────────────────────
shared_ptr<Frame> create_segmented_progress(int x, int y, int /*total_segments*/)
{
    // Figma 66:524 Group 156: a 202x5 progress bar drawn as a near-continuous
    // row of small 5px squares (square corners, ~2px gaps). Scaled x1.852 to
    // the panel. Each square is one child; update_segmented_progress_fraction()
    // colours the leading width green (so the first squares light up first).
    const int tick  = 9;                 // Figma 5px * SCALE
    const int gap   = 4;                 // Figma ~2px * SCALE
    const int pitch = tick + gap;        // 13
    const int n     = 29;                // fills ~374px (Figma 202 * SCALE)
    const int bar_w = n * pitch - gap;   // 373
    const int bar_h = tick;

    auto bar = make_shared<Frame>(Rect(x, y, bar_w, bar_h));
    bar->color(Palette::ColorId::bg, dt::kTransparent);
    bar->border(0);

    for (int i = 0; i < n; ++i)
    {
        auto sq = make_shared<Frame>(Rect(i * pitch, 0, tick, tick));
        sq->fill_flags({Theme::FillFlag::blend});
        sq->color(Palette::ColorId::bg, dt::kGrayLight);  // default: gray
        sq->border(0);            // square corners (no border_radius)
        bar->add(sq);
    }

    return bar;
}

void update_segmented_progress(
    shared_ptr<Frame> bar,
    int filled_segments,
    int total_segments)
{
    if (!bar) return;
    const int dots_per_segment = 5;
    int dot_index = 0;

    // Iterate children (each is a dot)
    for (auto& child : bar->children())
    {
        int seg = dot_index / dots_per_segment;
        auto* frame = dynamic_cast<Frame*>(child.get());
        if (frame) {
            if (seg < filled_segments)
                frame->color(Palette::ColorId::bg, dt::kGreen);
            else
                frame->color(Palette::ColorId::bg, dt::kGrayLight);
        }
        dot_index++;
    }
}

void update_segmented_progress_fraction(shared_ptr<Frame> bar, float fraction)
{
    if (!bar) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    // Width-proportional fill: a block is green if its centre falls within the
    // filled width. The leading fine ticks (narrow) light up first, then the
    // coarse blocks — matching Figma 66:524 where the fine ticks read as the
    // granular leading edge of progress.
    const float bar_w  = static_cast<float>(bar->content_area().width());
    const float fill_x = fraction * bar_w;
    for (auto& child : bar->children()) {
        auto* frame = dynamic_cast<Frame*>(child.get());
        if (!frame) continue;
        const float cx = frame->box().x() + frame->box().width() / 2.0f;
        frame->color(Palette::ColorId::bg,
                     (cx <= fill_x) ? dt::kGreen : dt::kGrayLight);
    }
}

// ── Linear Progress Bar ────────────────────────────────────────────────────
shared_ptr<Frame> create_linear_progress_bar(int x, int y, int width, int height)
{
    // Track (Figma: white fill with a thin gray outline, not a solid gray bar)
    auto track = make_shared<Frame>(Rect(x, y, width, height));
    track->fill_flags({Theme::FillFlag::blend});
    track->color(Palette::ColorId::bg, dt::kWhite);
    track->color(Palette::ColorId::border, Color(0xD9, 0xD9, 0xD9));   // #D9D9D9
    track->border(1);
    track->border_radius(height / 2);

    // Fill (green foreground)
    auto fill = make_shared<Frame>(Rect(0, 0, 0, height));
    fill->fill_flags({Theme::FillFlag::blend});
    fill->color(Palette::ColorId::bg, dt::kGreen);
    fill->border_radius(height / 2);
    fill->border(0);
    track->add(fill);

    return track;
}

void update_linear_progress(shared_ptr<Frame> bar, float percent)
{
    if (!bar) return;
    percent = std::clamp(percent, 0.0f, 100.0f);

    int track_w = bar->width();
    int track_h = bar->height();
    int fill_w = static_cast<int>(track_w * percent / 100.0f);

    // First child is the fill
    for (auto& child : bar->children())
    {
        auto* frame = dynamic_cast<Frame*>(child.get());
        if (frame) {
            frame->resize(Size(fill_w, track_h));
            break;
        }
    }
}

// ── Cumulative Time Footer ─────────────────────────────────────────────────
CumulativeTimeFooter create_cumulative_time_footer(int x, int y, int width)
{
    auto frame = make_shared<Frame>(Rect(x, y, width, 60));
    frame->color(Palette::ColorId::bg, dt::kTransparent);
    frame->border(0);

    // Horizontal line
    auto line = make_shared<Frame>(Rect(0, 0, width, 1));
    line->fill_flags({Theme::FillFlag::blend});
    line->color(Palette::ColorId::bg, dt::kGrayLight);
    line->border(0);
    frame->add(line);

    // Time label (left)
    auto time_lbl = make_shared<Label>("00:00",
        Rect(0, 8, width / 2, 30), AlignFlag::left);
    time_lbl->font(Font(24, Font::Weight::bold));
    time_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    frame->add(time_lbl);

    // Description (right)
    auto desc = make_shared<Label>("Cumulative treatment time",
        Rect(width / 2, 8, width / 2, 30), AlignFlag::right);
    desc->font(dt::fontSmall());
    desc->color(Palette::ColorId::label_text, dt::kTextPrimary);
    frame->add(desc);

    return {frame, time_lbl};
}

// ── Demo Mode Badge ────────────────────────────────────────────────────────
// Painter-drawn left "exit/leave" arrow (shaft + arrowhead). Font-independent
// so it renders on the target, where the ↩ unicode glyph was coming up blank.
namespace {
class ExitArrow : public Widget {
public:
    ExitArrow(const Rect& rect, const Color& col) : Widget(rect), m_col(col) {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }
    void draw(Painter& p, const Rect&) override {
        auto b = content_area();
        const float sz = static_cast<float>(std::min(b.width(), b.height()));
        const float cy = b.y() + b.height() / 2.0f;
        const float cx = b.x() + b.width()  / 2.0f;
        const float x0 = cx - sz * 0.30f;   // arrow tip (left)
        const float x1 = cx + sz * 0.30f;   // shaft end (right)
        const float head = sz * 0.22f;
        p.set(m_col);
        p.line_width(std::max(3.0f, sz * 0.11f));
        p.draw(Line(PointF(x0, cy), PointF(x1, cy)));       // shaft
        p.stroke();
        p.draw(Line(PointF(x0, cy), PointF(x0 + head, cy - head)));  // head ↖
        p.stroke();
        p.draw(Line(PointF(x0, cy), PointF(x0 + head, cy + head)));  // head ↙
        p.stroke();
    }
private:
    Color m_col;
};
} // namespace

// Two visual variants - see create_demo_mode_badge docstring in components.h.
// Treatment uses Card (floating card with border/shadow, prominent Exit so
// the user can always abort a running cycle). Patient-info uses Compact
// (clean labels + small button, no card chrome) so the badge doesn't
// compete with the form steps for attention.
DemoModeBadge create_demo_mode_badge(int x, int y, function<void()> on_leave,
                                     DemoBadgeStyle style)
{
    if (style == DemoBadgeStyle::Card) {
        // Figma 66:524: "DEMO MODE" floats as 50%-opacity cyan text (no card
        // chrome), with a separate white exit button (cyan border + cyan
        // arrow) below it. Sizes are Figma values * SCALE.
        const int badge_w = 150;
        const int badge_h = 140;
        auto frame = make_shared<Frame>(Rect(x, y, badge_w, badge_h));
        frame->fill_flags({});          // transparent — no card behind the text
        frame->border(0);

        const Color cyan_50(48, 163, 196, 128);   // Figma cyan @ 50% opacity

        auto demo = make_shared<Label>("DEMO",
            Rect(0, 0, badge_w, 40), AlignFlag::center);
        demo->font(Font(37, Font::Weight::normal));   // Figma 20pt Medium * SCALE
        demo->color(Palette::ColorId::label_text, cyan_50);
        frame->add(demo);

        auto mode = make_shared<Label>("MODE",
            Rect(0, 38, badge_w, 40), AlignFlag::center);
        mode->font(Font(37, Font::Weight::normal));
        mode->color(Palette::ColorId::label_text, cyan_50);
        frame->add(mode);

        // Exit button — the real Figma "bt leave" PNG (84:565), the same asset
        // the Demo Info screen uses. Never draw the arrow by hand.
        const int btn_w = 119, btn_h = 52;       // PNG 128x56 -> device px
        const int btn_x = (badge_w - btn_w) / 2;
        auto leave_btn = make_shared<Frame>(Rect(btn_x, 88, btn_w, btn_h));
        leave_btn->fill_flags({});               // transparent — PNG carries box + shadow
        try {
            const std::string path = "assets/figma/images/demo-info-btn-exit.png";
            auto probe = Image(("file:" + path).c_str());
            const float hs = static_cast<float>(btn_w) / probe.width();
            const float vs = static_cast<float>(btn_h) / probe.height();
            auto img = Image(("file:" + path).c_str(), hs, vs);
            auto lbl = make_shared<ImageLabel>(img);
            lbl->autoresize(false);
            lbl->border(0); lbl->padding(0); lbl->margin(0);
            lbl->fill_flags({});
            lbl->image_align(AlignFlag::center);
            lbl->box(Rect(0, 0, btn_w, btn_h));
            leave_btn->add(lbl);
        } catch (const std::exception& e) {
            printf("[BADGE] exit icon missing: %s\n", e.what()); fflush(stdout);
        }

        if (on_leave) {
            leave_btn->on_event([on_leave](Event& e) {
                if (e.id() == EventId::pointer_click) on_leave();
            }, {EventId::pointer_click});
        }
        frame->add(leave_btn);

        return {frame, nullptr};
    }

    // ── Compact variant ────────────────────────────────────────────────
    // Figma 2009:1156 (demo-mode patient steps): "DEMO MODE" in cyan @ 50%
    // opacity over the real "bt leave" PNG button (white box, cyan border,
    // cyan leave glyph). Smaller than the treatment Card badge. Never draw
    // the glyph by hand - use the downloaded PNG.
    // Figma block (2009:1353): 55x49.43 -> 102x92, DEMO/MODE lines ~28 apart
    // (line height 15.32px * SCALE), leave button right under them at y~61.
    const int badge_w = 104;
    const int badge_h = 92;
    auto frame = make_shared<Frame>(Rect(x, y, badge_w, badge_h));
    frame->fill_flags({});            // transparent - no card behind the text
    frame->border(0);

    const Color cyan_50(48, 163, 196, 128);   // Figma cyan #30A3C4 @ 50%

    auto demo = make_shared<Label>("DEMO",
        Rect(0, 0, badge_w, 28), AlignFlag::center);
    demo->font(Font(26, Font::Weight::normal));   // Figma 13.92pt Medium * SCALE
    demo->color(Palette::ColorId::label_text, cyan_50);
    frame->add(demo);

    auto mode = make_shared<Label>("MODE",
        Rect(0, 28, badge_w, 28), AlignFlag::center);
    mode->font(Font(26, Font::Weight::normal));
    mode->color(Palette::ColorId::label_text, cyan_50);
    frame->add(mode);

    // Exit button - the real Figma "bt leave" PNG (84:565), the same asset the
    // Demo Info screen and the treatment Card badge use. The PNG already
    // carries the white box, cyan border and cyan leave glyph.
    const int btn_w = 77, btn_h = 31;
    const int btn_x = (badge_w - btn_w) / 2;
    auto leave_btn = make_shared<Frame>(Rect(btn_x, 61, btn_w, btn_h));
    leave_btn->fill_flags({});               // transparent - PNG carries box + border
    try {
        const std::string path = "assets/figma/images/demo-info-btn-exit.png";
        auto probe = Image(("file:" + path).c_str());
        const float hs = static_cast<float>(btn_w) / probe.width();
        const float vs = static_cast<float>(btn_h) / probe.height();
        auto img = Image(("file:" + path).c_str(), hs, vs);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, btn_w, btn_h));
        leave_btn->add(lbl);
    } catch (const std::exception& e) {
        printf("[BADGE] exit icon missing: %s\n", e.what()); fflush(stdout);
    }

    if (on_leave) {
        leave_btn->on_event([on_leave](Event& e) {
            if (e.id() == EventId::pointer_click) on_leave();
        }, {EventId::pointer_click});
    }
    frame->add(leave_btn);

    // Compact returns the Frame in the leave_btn slot - caller treats it
    // as an opaque handle.
    return {frame, nullptr};
}

// ── Error Overlay ──────────────────────────────────────────────────────────
shared_ptr<Frame> create_error_overlay(
    ErrorSeverity severity,
    const string& title,
    const string& message,
    const string& error_code,
    function<void()> on_dismiss)
{
    // Full-screen semi-transparent overlay
    auto overlay = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    overlay->fill_flags({Theme::FillFlag::blend});
    overlay->color(Palette::ColorId::bg, Color(0, 0, 0, 80));
    overlay->border(0);

    // Card (centered)
    const int card_w = 720;
    const int card_h = 400;
    auto card = make_shared<Frame>(
        Rect((dt::SCREEN_W - card_w) / 2, (dt::SCREEN_H - card_h) / 2, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kWhite);
    card->border_radius(dt::RADIUS_LG);
    card->border(0);
    overlay->add(card);

    // Severity banner (top colored bar)
    Color banner_color;
    string severity_text;
    switch (severity) {
        case ErrorSeverity::Informational:
            banner_color = dt::kErrorInfoBanner;
            severity_text = "Information";
            break;
        case ErrorSeverity::Warning:
            banner_color = dt::kErrorWarnBanner;
            severity_text = "Warning";
            break;
        case ErrorSeverity::CriticalFault:
            banner_color = dt::kErrorCritBanner;
            severity_text = "Critical Fault";
            break;
    }

    auto banner = make_shared<Frame>(Rect(0, 0, card_w, 80));
    banner->fill_flags({Theme::FillFlag::blend});
    banner->color(Palette::ColorId::bg, banner_color);
    banner->border(0);
    // Note: top-only border_radius not directly supported - use full radius on banner
    card->add(banner);

    auto sev_label = make_shared<Label>(severity_text,
        Rect(0, 0, card_w, 80));
    sev_label->font(Font(24, Font::Weight::bold));
    sev_label->color(Palette::ColorId::label_text, dt::kWhite);
    banner->add(sev_label);

    // Title
    auto title_lbl = make_shared<Label>(title,
        Rect(30, 100, card_w - 60, 40));
    title_lbl->font(dt::fontTitle());
    title_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(title_lbl);

    // Message
    auto msg_lbl = make_shared<Label>(message,
        Rect(30, 150, card_w - 60, 100));
    msg_lbl->font(dt::fontBody());
    msg_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(msg_lbl);

    // Error code (if provided)
    if (!error_code.empty()) {
        auto code_lbl = make_shared<Label>("Error Code: " + error_code,
            Rect(30, 260, card_w - 60, 30));
        code_lbl->font(dt::fontSmall());
        code_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        card->add(code_lbl);
    }

    // Dismiss button (only for non-critical)
    if (severity != ErrorSeverity::CriticalFault && on_dismiss) {
        auto dismiss_btn = create_filled_button("Dismiss",
            Rect((card_w - 200) / 2, card_h - 80, 200, 50),
            on_dismiss);
        card->add(dismiss_btn);
    }

    return overlay;
}

// ── Chevron-left glyph ─────────────────────────────────────────────────────
ChevronLeft::ChevronLeft(const Rect& rect) : Widget(rect)
{
    fill_flags({Theme::FillFlag::blend});
    border(0);
}

void ChevronLeft::draw(Painter& painter, const Rect&)
{
    auto b = content_area();
    float cx = b.x() + b.width()  / 2.0f;
    float cy = b.y() + b.height() / 2.0f;
    float dim = static_cast<float>(min(b.width(), b.height()));
    const float half_w = dim * 0.18f;
    const float half_h = dim * 0.26f;
    painter.set(dt::kTextPrimary);
    painter.line_width(max(3.5f, dim * 0.07f));
    painter.draw(Line(Point(cx + half_w, cy - half_h),
                      Point(cx - half_w, cy)));
    painter.stroke();
    painter.draw(Line(Point(cx - half_w, cy),
                      Point(cx + half_w, cy + half_h)));
    painter.stroke();
}

// ── Back button (standard bottom-left, identical across screens) ──────────
// Renders the Figma "bt EXIT" component (84:565 / 2073:1839) as a PNG so the
// art is pixel-equivalent to the design. Same visual and same on-screen
// position as DEMO_INFO and WIFI_UNAVAILABLE.
shared_ptr<Frame> add_back_button(Frame& container, function<void()> on_click)
{
    // Figma button bbox is 84x33 figma -> 156x61 device at frame-local
    // (14, 216) -> (26, 400). The exported PNG carries the gray circle, the
    // left chevron, and the "Back" text with its drop shadow padding; its
    // natural device size (PNG_px * SCALE / 2) is 209x98. Top-left placed
    // so the visible button portion centres on the figma button centre.
    const std::string png_path = "assets/figma/images/demo-info-btn-back.png";
    auto wrap = make_shared<Frame>(Rect(0, 381, 209, 98));
    wrap->fill_flags({});

    try {
        auto probe = Image(("file:" + png_path).c_str());
        const float hs = static_cast<float>(wrap->width())  / probe.width();
        const float vs = static_cast<float>(wrap->height()) / probe.height();
        auto img = Image(("file:" + png_path).c_str(), hs, vs);
        auto lbl = make_shared<ImageLabel>(img);
        lbl->autoresize(false);
        lbl->border(0); lbl->padding(0); lbl->margin(0);
        lbl->fill_flags({});
        lbl->image_align(AlignFlag::center);
        lbl->box(Rect(0, 0, wrap->width(), wrap->height()));
        wrap->add(lbl);
    } catch (const std::exception& e) {
        printf("[BACK_BTN] image %s missing: %s\n", png_path.c_str(), e.what());
        fflush(stdout);
    }

    wrap->on_event([on_click](Event& e) {
        if (e.id() == EventId::pointer_click && on_click) on_click();
    });

    container.add(wrap);
    return wrap;
}

} // namespace ui
