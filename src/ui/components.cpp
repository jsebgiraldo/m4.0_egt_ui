#include "components.h"
#include "design_tokens.h"
#include "palette.h"
#include "../generated/embedded_assets.h"
#include <cmath>
#include <fstream>

using namespace egt;
using namespace std;

namespace ui {

// Write embedded PNG to temp file once, return path
static string get_logo_path()
{
    static const string path = "/tmp/egt-logo.png";
    static bool written = false;
    if (!written)
    {
        ofstream f(path, ios::binary);
        f.write(reinterpret_cast<const char*>(assets_image_Lice_logo_png),
                assets_image_Lice_logo_png_len);
        written = f.good();
        printf("[LOGO] wrote %u bytes to %s: %s\n",
               assets_image_Lice_logo_png_len, path.c_str(),
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
// Compact 2-line "DEMO / MODE" label on top + small arrow-icon exit button
// below. Matches Figma 67:578 — the prior version was a big gray "Exit"
// pill that drew too much attention away from the treatment content.
DemoModeBadge create_demo_mode_badge(int x, int y, function<void()> on_leave)
{
    const int badge_w = 100;
    const int badge_h = 90;
    auto frame = make_shared<Frame>(Rect(x, y, badge_w, badge_h));
    frame->color(Palette::ColorId::bg, dt::kTransparent);
    frame->border(0);

    // "DEMO" on top
    auto demo = make_shared<Label>("DEMO",
        Rect(0, 0, badge_w, 24), AlignFlag::center);
    demo->font(Font(16, Font::Weight::bold));
    demo->color(Palette::ColorId::label_text, dt::kAccentCyan);
    frame->add(demo);

    // "MODE" below
    auto mode = make_shared<Label>("MODE",
        Rect(0, 22, badge_w, 24), AlignFlag::center);
    mode->font(Font(16, Font::Weight::bold));
    mode->color(Palette::ColorId::label_text, dt::kAccentCyan);
    frame->add(mode);

    // Small exit-icon button — rounded white square with a leave arrow.
    // The arrow is a unicode character so we don't need a separate SVG.
    const int btn_w = 44, btn_h = 32;
    const int btn_x = (badge_w - btn_w) / 2;
    auto leave_btn = make_shared<Button>("↪",   // ↪ "leftwards arrow with hook"
        Rect(btn_x, 52, btn_w, btn_h));
    leave_btn->font(Font(18, Font::Weight::bold));
    leave_btn->color(Palette::ColorId::button_bg, dt::kWhite);
    leave_btn->color(Palette::ColorId::button_text, dt::kAccentCyan);
    leave_btn->color(Palette::ColorId::border, dt::kGrayLight);
    leave_btn->border_radius(dt::RADIUS_XS);
    leave_btn->border(1);
    if (on_leave) {
        leave_btn->on_click([on_leave](Event&) { on_leave(); });
    }
    frame->add(leave_btn);

    return {frame, leave_btn};
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
    // Note: top-only border_radius not directly supported — use full radius on banner
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

    // Hit zone — transparent overlay covering the whole wrap.
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
