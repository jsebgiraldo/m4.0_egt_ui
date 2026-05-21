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
shared_ptr<Frame> create_segmented_progress(int x, int y, int total_segments)
{
    // Each segment is a group of 5 small dots. In Figma: 32px wide per group, 5 dots of 5×5.
    // At 800×480 scale: each group ~60px wide, dots ~9×9.
    const int grp_w = dt::SEGMENT_W;
    const int gap_between_groups = dt::SEGMENT_GAP;
    const int total_w = total_segments * grp_w + (total_segments - 1) * gap_between_groups;

    auto bar = make_shared<Frame>(Rect(x, y, total_w, dt::SEGMENT_DOT_SZ));
    bar->color(Palette::ColorId::bg, dt::kTransparent);
    bar->border(0);

    const int dots_per_segment = 5;
    const int dot_sz = dt::SEGMENT_DOT_SZ;
    const int dot_gap = (grp_w - dots_per_segment * dot_sz) / (dots_per_segment - 1);

    for (int seg = 0; seg < total_segments; seg++)
    {
        int seg_x = seg * (grp_w + gap_between_groups);
        for (int d = 0; d < dots_per_segment; d++)
        {
            auto dot = make_shared<Frame>(
                Rect(seg_x + d * (dot_sz + dot_gap), 0, dot_sz, dot_sz));
            dot->fill_flags({Theme::FillFlag::blend});
            dot->color(Palette::ColorId::bg, dt::kGrayLight);  // default: gray
            dot->border_radius(dot_sz / 2);
            dot->border(0);
            bar->add(dot);
        }
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

    const int total = static_cast<int>(bar->children().size());
    const int filled = static_cast<int>(std::lround(fraction * total));
    int i = 0;
    for (auto& child : bar->children()) {
        auto* frame = dynamic_cast<Frame*>(child.get());
        if (frame)
            frame->color(Palette::ColorId::bg,
                         (i < filled) ? dt::kGreen : dt::kGrayLight);
        i++;
    }
}

// ── Linear Progress Bar ────────────────────────────────────────────────────
shared_ptr<Frame> create_linear_progress_bar(int x, int y, int width, int height)
{
    // Track (gray background)
    auto track = make_shared<Frame>(Rect(x, y, width, height));
    track->fill_flags({Theme::FillFlag::blend});
    track->color(Palette::ColorId::bg, dt::kGrayLight);
    track->border_radius(height / 2);
    track->border(0);

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
        const int badge_w = 150;
        const int badge_h = 134;
        auto frame = make_shared<Frame>(Rect(x, y, badge_w, badge_h));
        frame->fill_flags({Theme::FillFlag::blend});
        frame->color(Palette::ColorId::bg, dt::kWhite);
        frame->color(Palette::ColorId::border, palette::kGray400);
        frame->border(1);
        frame->border_radius(dt::RADIUS_SM);
        frame->border_flags({Theme::BorderFlag::drop_shadow});

        auto demo = make_shared<Label>("DEMO",
            Rect(0, 10, badge_w, 28), AlignFlag::center);
        demo->font(Font(22, Font::Weight::bold));
        demo->color(Palette::ColorId::label_text, dt::kAccentCyan);
        frame->add(demo);

        auto mode = make_shared<Label>("MODE",
            Rect(0, 38, badge_w, 28), AlignFlag::center);
        mode->font(Font(22, Font::Weight::bold));
        mode->color(Palette::ColorId::label_text, dt::kAccentCyan);
        frame->add(mode);

        // Frame + centered Label, click handler - same approach as
        // Compact to avoid egt::Button's notch artifact at small sizes.
        const int btn_w = 84, btn_h = 42;
        const int btn_x = (badge_w - btn_w) / 2;
        auto leave_btn = make_shared<Frame>(Rect(btn_x, 78, btn_w, btn_h));
        leave_btn->fill_flags({Theme::FillFlag::blend});
        leave_btn->color(Palette::ColorId::bg, dt::kAccentCyan);
        leave_btn->color(Palette::ColorId::border, dt::kAccentCyan);
        leave_btn->border_radius(dt::RADIUS_SM);
        leave_btn->border(0);

        leave_btn->add(make_shared<ExitArrow>(
            Rect(0, 0, btn_w, btn_h), dt::kWhite));

        if (on_leave) {
            leave_btn->on_event([on_leave](Event& e) {
                if (e.id() == EventId::pointer_click) on_leave();
            }, {EventId::pointer_click});
        }
        frame->add(leave_btn);

        return {frame, nullptr};
    }

    // ── Compact variant ────────────────────────────────────────────────
    const int badge_w = 90;
    const int badge_h = 80;
    auto frame = make_shared<Frame>(Rect(x, y, badge_w, badge_h));
    frame->color(Palette::ColorId::bg, dt::kTransparent);
    frame->border(0);

    auto demo = make_shared<Label>("DEMO",
        Rect(0, 0, badge_w, 20), AlignFlag::center);
    demo->font(Font(14, Font::Weight::bold));
    demo->color(Palette::ColorId::label_text, dt::kAccentCyan);
    frame->add(demo);

    auto mode = make_shared<Label>("MODE",
        Rect(0, 18, badge_w, 20), AlignFlag::center);
    mode->font(Font(14, Font::Weight::bold));
    mode->color(Palette::ColorId::label_text, dt::kAccentCyan);
    frame->add(mode);

    // Exit button - built from a Frame (full radius/fill control) with
    // a Label glyph on top, instead of egt::Button. The Button widget
    // draws extra theme passes (focus ring + active overlay) that don't
    // respect border_radius at small sizes, producing a "bite" in the
    // bottom-right corner.
    const int btn_w = 56, btn_h = 32;
    const int btn_x = (badge_w - btn_w) / 2;
    auto leave_btn = make_shared<Frame>(Rect(btn_x, 44, btn_w, btn_h));
    leave_btn->fill_flags({Theme::FillFlag::blend});
    leave_btn->color(Palette::ColorId::bg, dt::kAccentCyan);
    leave_btn->color(Palette::ColorId::border, dt::kAccentCyan);
    leave_btn->border_radius(dt::RADIUS_SM);
    leave_btn->border(0);

    leave_btn->add(make_shared<ExitArrow>(
        Rect(0, 0, btn_w, btn_h), dt::kWhite));

    if (on_leave) {
        leave_btn->on_event([on_leave](Event& e) {
            if (e.id() == EventId::pointer_click) on_leave();
        }, {EventId::pointer_click});
    }
    frame->add(leave_btn);

    // Compact returns the Frame in the leave_btn slot - caller treats it
    // as an opaque handle, doesn't care it's not a Button anymore.
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
shared_ptr<Frame> add_back_button(Frame& container, function<void()> on_click)
{
    // Coordinates from Figma 2073:1996 (Settings screen reference).
    constexpr int circle_d = 46;
    constexpr int back_x   = 27;
    constexpr int back_y   = 414;
    constexpr int chev_w   = 30;
    constexpr int chev_h   = 44;
    constexpr int gap      = 10;

    // Wrapper covering all back widgets so the caller can toggle the whole
    // affordance visible/invisible as a unit (e.g. when an overlay is shown).
    auto wrap = make_shared<Frame>(
        Rect(back_x - 6, back_y - 4, circle_d + gap + 120 + 12, circle_d + 8));
    wrap->fill_flags({});
    wrap->color(Palette::ColorId::bg, dt::kTransparent);
    wrap->border(0);
    container.add(wrap);

    // All children below position relative to the wrap origin.
    constexpr int wrap_x_offset = 6;
    constexpr int wrap_y_offset = 4;

    auto circle = make_shared<Frame>(
        Rect(wrap_x_offset, wrap_y_offset, circle_d, circle_d));
    circle->fill_flags({Theme::FillFlag::blend});
    circle->color(Palette::ColorId::bg, palette::kGray200);
    circle->border(0);
    circle->border_radius(circle_d / 2);
    wrap->add(circle);

    auto chev = make_shared<ChevronLeft>(
        Rect(wrap_x_offset + (circle_d - chev_w) / 2,
             wrap_y_offset + (circle_d - chev_h) / 2,
             chev_w, chev_h));
    wrap->add(chev);

    auto lbl = make_shared<Label>("Back",
        Rect(wrap_x_offset + circle_d + gap, wrap_y_offset, 120, circle_d));
    lbl->font(Font(15, Font::Weight::bold));
    lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    lbl->text_align(AlignFlag::left | AlignFlag::center_vertical);
    wrap->add(lbl);

    // Hit zone - transparent overlay covering the whole wrap.
    auto hit = make_shared<Frame>(
        Rect(0, 0, wrap->width(), wrap->height()));
    hit->fill_flags({Theme::FillFlag::blend});
    hit->color(Palette::ColorId::bg, dt::kTransparent);
    hit->border(0);
    wrap->add(hit);
    hit->on_event([on_click](Event&) {
        if (on_click) on_click();
    }, {EventId::pointer_click});

    return wrap;
}

} // namespace ui
