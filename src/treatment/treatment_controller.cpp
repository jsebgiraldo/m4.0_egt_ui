#include "treatment_controller.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace egt;
using namespace std;

// ── Green frame border for "nearly done" screen ────────────────────────────
// Matches Figma node 67:578: a chunky green band around the perimeter with
// a white interior, instead of a full-screen green wash. Signals "treatment
// nearly finished" without taking over the whole UI.
class GreenFrameBorder : public Widget {
public:
    explicit GreenFrameBorder(const Rect& rect, int border_w = 14)
        : Widget(rect), m_border_w(border_w)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        // Outer green rectangle (the visible band).
        painter.set(dt::kGreen);
        painter.draw(b);
        painter.fill();
        // Inner white rectangle — everything inside this stays white so
        // text and dots render exactly like a normal treatment screen.
        const int bw = m_border_w;
        Rect inner(b.x() + bw, b.y() + bw,
                   b.width() - 2 * bw, b.height() - 2 * bw);
        painter.set(dt::kWhite);
        painter.draw(inner);
        painter.fill();
    }

private:
    int m_border_w;
};

// Dashed progress bar used on the nearly-finished screen. Renders as a
// row of N small green dashes with the trailing M dashes greyed out to
// indicate remaining time. Matches the Figma styling (5 groups of small
// rectangles, last group fading).
class DashedProgressBar : public Widget {
public:
    DashedProgressBar(const Rect& rect, int total_dashes, int filled_dashes)
        : Widget(rect), m_total(total_dashes), m_filled(filled_dashes)
    {
        fill_flags({Theme::FillFlag::blend});
        border(0);
    }

    void set_filled(int n) {
        n = std::max(0, std::min(m_total, n));
        if (n != m_filled) { m_filled = n; damage(); }
    }

    void draw(Painter& painter, const Rect& /*rect*/) override
    {
        auto b = content_area();
        const int dash_w = 10;
        const int dash_h = 4;
        const int small_gap = 4;
        const int total_w = m_total * dash_w + (m_total - 1) * small_gap;
        const int start_x = b.x() + (b.width() - total_w) / 2;
        const int dash_y  = b.y() + (b.height() - dash_h) / 2;
        for (int i = 0; i < m_total; i++) {
            int x = start_x + i * (dash_w + small_gap);
            Color c = (i < m_filled) ? dt::kGreen : Color(210, 210, 210);
            painter.set(c);
            painter.draw(Rect(x, dash_y, dash_w, dash_h));
            painter.fill();
        }
    }

private:
    int m_total, m_filled;
};

// ── Shared state across treatment screens ───────────────────────────────────
struct TreatmentState {
    TreatmentConfig config;
    TreatmentCallbacks callbacks;

    int cumulative_seconds = 0;    // total treatment time accumulated
    int current_cycle = 0;         // which cycle we're on (0-based)
    int cycles_completed = 0;      // how many full cycles done

    bool is_paused = false;

    // Timer references (kept alive via shared_ptr)
    shared_ptr<PeriodicTimer> active_timer;

    // One-shot guard: once the user exits (Exit / End Treatment / Complete)
    // every in-flight timer must early-out so it doesn't yank the user back
    // into the treatment flow seconds after they navigated away.
    shared_ptr<bool> alive = make_shared<bool>(true);

    // Helper: format seconds as mm:ss
    static string format_time(int seconds) {
        int m = seconds / 60;
        int s = seconds % 60;
        stringstream ss;
        ss << setfill('0') << setw(2) << m << ":" << setw(2) << s;
        return ss.str();
    }

    // How many total cycles to reach target
    int total_cycles() const {
        if (config.cycle_seconds <= 0) return 1;
        return max(1, (config.total_target_seconds + config.cycle_seconds - 1) / config.cycle_seconds);
    }

    // Is treatment complete?
    bool is_complete() const {
        return cumulative_seconds >= config.total_target_seconds;
    }
};

// ── Forward declarations ────────────────────────────────────────────────────
static void show_warming(shared_ptr<TreatmentState> state);
static void show_ready(shared_ptr<TreatmentState> state);
static void show_position_tip(shared_ptr<TreatmentState> state);
static void show_treatment_active(shared_ptr<TreatmentState> state);
static void show_treatment_paused(shared_ptr<TreatmentState> state);
static void show_end_confirmation(shared_ptr<TreatmentState> state);
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early);
static void show_treatment_nearly_done(shared_ptr<TreatmentState> state, int remaining_seconds);

// ── Container result with dynamic labels ────────────────────────────────────
struct TreatmentScreen {
    shared_ptr<Frame> container;
    shared_ptr<Label> cumulative_time_label;  // for dynamic updates
};

// ── Figma-matched layout constants (432×261 → 800×480) ─────────────────────
// Cumulative time header area (Figma: Group 159 at y=23, line at y=56)
static constexpr int CUM_TIME_Y     = 15;   // y for time labels
static constexpr int CUM_SEP_Y      = 110;  // y for separator line (below logo at y=5+103=108)
static constexpr int CUM_LEFT_X     = 180;  // x start (right of logo at x=10+166=176)
static constexpr int CUM_WIDTH      = 400;  // width of cumulative area (up to demo badge at x=580)

// Main content area
static constexpr int CONTENT_Y      = 120;  // y for large number/percentage
static constexpr int CONTENT_H      = 145;  // height of large number area
static constexpr int STATUS_Y       = 275;  // y for status text below countdown
static constexpr int DOTS_Y         = 325;  // y for segmented progress dots
static constexpr int BTN_Y          = 390;  // y for bottom buttons
static constexpr int BTN_W          = 220;  // button width
static constexpr int BTN_H          = 80;   // button height
static constexpr int BTN_LEFT_X     = 40;   // left button x
static constexpr int BTN_RIGHT_X    = 540;  // right button x

// ── Common helpers ──────────────────────────────────────────────────────────
static TreatmentScreen make_treatment_container(
    shared_ptr<TreatmentState> state,
    bool show_cumulative,
    bool green_mode = false)
{
    // green_mode now means "nearly-finished frame" — same dark text and
    // white bg as normal screens, just with a green band around the edges.
    // (The old behaviour was a full-screen green wash with white text;
    // see Figma 67:578 for the updated treatment.)
    const Color text_color  = dt::kTextPrimary;
    const Color sep_color   = dt::kGrayLight;
    const Color bg_color    = dt::kBgWhite;

    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, bg_color);

    if (green_mode) {
        auto frame = make_shared<GreenFrameBorder>(
            Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
        container->add(frame);
    }

    // Logo (top-left, full Figma size)
    auto logo = ui::create_logo(10, 5, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Demo mode badge (top-right, vertical: DEMO MODE label + Exit below)
    if (state->config.demo_mode) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 150, 5, state->callbacks.on_leave_to_home);
        container->add(badge.frame);
    }

    shared_ptr<Label> cum_time_lbl;

    // Cumulative time in header area (Figma: between logo and badge)
    if (show_cumulative) {
        // Separator line
        auto sep = make_shared<Frame>(Rect(CUM_LEFT_X, CUM_SEP_Y, CUM_WIDTH, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, sep_color);
        sep->border(0);
        container->add(sep);

        // Time value (right side)
        cum_time_lbl = make_shared<Label>(
            TreatmentState::format_time(state->cumulative_seconds),
            Rect(CUM_LEFT_X + CUM_WIDTH / 2, CUM_TIME_Y, CUM_WIDTH / 2, 35),
            AlignFlag::right);
        cum_time_lbl->font(Font(24, Font::Weight::bold));
        cum_time_lbl->color(Palette::ColorId::label_text, text_color);
        container->add(cum_time_lbl);

        // Description (left side)
        auto desc = make_shared<Label>("Cumulative treatment time",
            Rect(CUM_LEFT_X, CUM_TIME_Y + 3, CUM_WIDTH / 2, 30),
            AlignFlag::left);
        desc->font(Font(18, Font::Weight::normal));
        desc->color(Palette::ColorId::label_text, text_color);
        container->add(desc);
    }

    return {container, cum_time_lbl};
}

// Two-tier action button used across treatment screens: the verb
// ("Pause" / "End" / "Resume") is the dominant text, "Treatment" below it
// is small and secondary. The previous `create_outlined_button("Pause\n…")`
// rendered both lines at the same size — the verb (the actually useful
// affordance) got lost in the wall of text. Style is passed in so the
// same shape works for outlined, green-filled, cyan-filled, and white-on-
// green variants the treatment screens use.
struct ActionBtnStyle {
    Color bg;
    Color fg;
    Color border;
    int   border_width;
};

static const ActionBtnStyle BTN_OUTLINED        = { dt::kWhite,      dt::kTextPrimary, dt::kGrayLight, 2 };
static const ActionBtnStyle BTN_GREEN_FILLED    = { dt::kGreen,      dt::kWhite,       dt::kGreen,     0 };
static const ActionBtnStyle BTN_CYAN_FILLED     = { dt::kAccentCyan, dt::kWhite,       dt::kAccentCyan,0 };
static const ActionBtnStyle BTN_WHITE_GREEN_FG  = { dt::kWhite,      dt::kGreen,       dt::kWhite,     0 };

static shared_ptr<Frame> make_action_button(
    const string& big_text,
    const string& small_text,
    const Rect& rect,
    const ActionBtnStyle& style,
    function<void()> on_click)
{
    auto frame = make_shared<Frame>(rect);
    frame->fill_flags({Theme::FillFlag::blend});
    frame->color(Palette::ColorId::bg, style.bg);
    frame->color(Palette::ColorId::border, style.border);
    frame->border(style.border_width);
    frame->border_radius(dt::RADIUS_MD);

    // Big verb (top) — 30 pt bold dominates the visual weight.
    auto big = make_shared<Label>(big_text,
        Rect(0, 10, rect.width(), 38),
        AlignFlag::center);
    big->font(Font(30, Font::Weight::bold));
    big->color(Palette::ColorId::label_text, style.fg);
    frame->add(big);

    // Small qualifier — 14 pt regular, sits below the verb.
    auto small = make_shared<Label>(small_text,
        Rect(0, 48, rect.width(), 22),
        AlignFlag::center);
    small->font(Font(14, Font::Weight::normal));
    small->color(Palette::ColorId::label_text, style.fg);
    frame->add(small);

    if (on_click) {
        frame->on_event([on_click](Event& e) {
            if (e.id() == EventId::pointer_click) on_click();
        }, {EventId::pointer_click});
    }
    return frame;
}

// Helper: add segmented progress dots at standard y position
static void add_segmented_progress(
    shared_ptr<Frame> container,
    shared_ptr<TreatmentState> state)
{
    int seg_total = state->total_cycles();
    int seg_filled = state->cycles_completed;
    int seg_display = min(seg_total, dt::SEGMENT_COUNT);
    int filled_display = 0;
    if (seg_total > 0)
        filled_display = min(seg_display,
            (int)round((double)seg_filled / seg_total * seg_display));

    auto seg_bar = ui::create_segmented_progress(
        (dt::SCREEN_W - (seg_display * (dt::SEGMENT_W + dt::SEGMENT_GAP) - dt::SEGMENT_GAP)) / 2,
        DOTS_Y,
        seg_display);
    ui::update_segmented_progress(seg_bar, filled_display, seg_display);
    container->add(seg_bar);
}

// ── Entry point ─────────────────────────────────────────────────────────────
void start_treatment_flow(
    const TreatmentConfig& config,
    const TreatmentCallbacks& callbacks)
{
    auto state = make_shared<TreatmentState>();
    state->config = config;
    state->callbacks = callbacks;

    // Wrap every exit callback so it (1) trips the one-shot guard, (2) cancels
    // the current phase timer, (3) only then calls the host's handler. Without
    // this, the user could tap Exit / Complete / End Treatment and a pending
    // timer tick fired ~1 s later would re-call show_treatment_active and yank
    // them back into the treatment flow on top of whatever screen they're on.
    auto wrap_exit = [state](function<void()> cb) -> function<void()> {
        if (!cb) return cb;
        return [state, cb]() {
            if (!*state->alive) return;       // already exited — ignore re-entry
            *state->alive = false;
            if (state->active_timer) state->active_timer->cancel();
            cb();
        };
    };
    state->callbacks.on_leave_to_home        = wrap_exit(state->callbacks.on_leave_to_home);
    state->callbacks.on_treatment_completed  = wrap_exit(state->callbacks.on_treatment_completed);
    state->callbacks.on_treatment_ended_early= wrap_exit(state->callbacks.on_treatment_ended_early);

    show_warming(state);
}

// ── Percentage layout helper ────────────────────────────────────────────────
// Lays out a big "NN%" composite (large bold number + smaller superscript %)
// centred horizontally on the screen, regardless of digit count. Until we
// did this, the number's rect was fixed and right-aligned at x=430 — fine
// at 1 digit but the whole "10%" / "100%" group drifted left as more digits
// appeared, throwing the visual centre off.
//
// Digit widths at 120 pt bold Lato — calibrated from rendered output:
// "1" comes out ~70 px wide, all other digits ~110 px. The "%" at 56 pt
// bold renders ~65 px. The estimates don't have to be perfectly tight —
// they just need to track the digit-count transitions so the composite
// re-centres cleanly as the warming counter rolls over 9→10 and 99→100.
static int pct_digit_width_120(char d) { return (d == '1') ? 70 : 110; }
static int pct_number_width_120(int v) {
    string s = to_string(v);
    int w = 0;
    for (char c : s) w += pct_digit_width_120(c);
    return w;
}

static void center_pct_group(int value,
                             shared_ptr<Label> num_label,
                             shared_ptr<Label> pct_label,
                             int num_y, int num_h, int pct_y)
{
    constexpr int NUM_PCT_GAP = 10;
    const int n_w = pct_number_width_120(value);

    // Centre the NUMBER on the screen — the % hangs off to the right as a
    // unit suffix. Centring the whole "NN%" composite geometrically looks
    // off-axis because the % is visually much lighter than the digits, so
    // the digits read as shifted left of centre. Apple Watch / iOS fitness
    // apps use this same convention: the value dominates, the unit floats.
    const int num_x = (dt::SCREEN_W - n_w) / 2;
    num_label->move(Point(num_x, num_y));
    num_label->resize(Size(n_w, num_h));
    num_label->text_align(AlignFlag::center);

    pct_label->move(Point(num_x + n_w + NUM_PCT_GAP, pct_y));
}

// ── WARMING SCREEN ──────────────────────────────────────────────────────────
// Figma Group 179: large % number + superscript %, two-line status, progress bar.
// No cumulative time, no segmented dots, no buttons.
static void show_warming(shared_ptr<TreatmentState> state)
{
    // Warming-screen-local layout (independent from other treatment screens)
    // Number font 120px → rendered height ~145px
    const int W_NUM_Y      = 120;  // top of big number area (below full-size logo)
    const int W_NUM_H      = 155;  // height of number rect (120px font)
    const int W_PCT_H      = 80;   // height of % rect
    const int W_PCT_Y      = W_NUM_Y + W_NUM_H - W_PCT_H;  // bottom-aligned with number
    const int W_STATUS1_Y  = 285;  // "Warming up"
    const int W_STATUS2_Y  = 313;  // "for Treatment"
    const int W_BAR_Y      = 405;  // progress bar y

    auto [container, _cum_lbl] = make_treatment_container(state, false);

    // ── Large number + "%" suffix at baseline, centred as a group ─────────
    // The "%" used to sit at the top (superscript style); we now bottom-
    // align it so it reads at the baseline of the digits, like "100%"
    // written normally. Position is re-computed each tick via
    // center_pct_group so the composite stays centred when digit count
    // changes.
    auto num_label = make_shared<Label>("0");
    num_label->font(Font(120, Font::Weight::bold));
    num_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(num_label);

    auto pct_sup = make_shared<Label>("%",
        Rect(0, W_PCT_Y, 90, W_PCT_H),
        AlignFlag::bottom | AlignFlag::left);
    pct_sup->font(Font(56, Font::Weight::bold));
    pct_sup->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(pct_sup);

    center_pct_group(0, num_label, pct_sup, W_NUM_Y, W_NUM_H, W_PCT_Y);

    // ── Two-line status text ──────────────────────────────────────────────
    auto status1 = make_shared<Label>("Warming up",
        Rect(0, W_STATUS1_Y, dt::SCREEN_W, 28));
    status1->font(Font(20, Font::Weight::normal));
    status1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status1);

    auto status2 = make_shared<Label>("for Treatment",
        Rect(0, W_STATUS2_Y, dt::SCREEN_W, 28));
    status2->font(Font(20, Font::Weight::normal));
    status2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status2);

    // ── Linear progress bar ───────────────────────────────────────────────
    const int bar_w = 700;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2,
        W_BAR_Y,
        bar_w, 10);
    container->add(progress_bar);

    state->callbacks.on_show_screen(container);

    // ── Animate warming over configured seconds ───────────────────────────
    auto progress_val = make_shared<float>(0.0f);
    auto elapsed_ms = make_shared<int>(0);
    const int total_ms = state->config.warming_seconds * 1000;

    auto timer = make_shared<PeriodicTimer>(chrono::milliseconds(50));
    state->active_timer = timer;

    weak_ptr<Label> w_num = num_label;
    weak_ptr<Label> w_pct = pct_sup;
    weak_ptr<Frame> w_bar = progress_bar;
    auto last_digits = make_shared<int>(1);  // re-centre only when digit count changes

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        *elapsed_ms += 50;
        *progress_val = min(100.0f, (*elapsed_ms * 100.0f) / total_ms);

        const int iv = static_cast<int>(*progress_val);
        if (auto lb = w_num.lock())
            lb->text(to_string(iv));

        const int digits = (iv >= 100) ? 3 : (iv >= 10) ? 2 : 1;
        if (digits != *last_digits) {
            *last_digits = digits;
            auto lb = w_num.lock(); auto pc = w_pct.lock();
            if (lb && pc)
                center_pct_group(iv, lb, pc, W_NUM_Y, W_NUM_H, W_PCT_Y);
        }

        if (auto bar = w_bar.lock())
            ui::update_linear_progress(bar, *progress_val);

        if (*elapsed_ms >= total_ms) {
            timer->cancel();
            show_ready(state);
        }
    });
    timer->start();
}

// ── READY SCREEN ────────────────────────────────────────────────────────────
// Figma REG_READY (168:810): "Ready for Treatment" + 100% + "Begin Treatment" button.
// User must press Begin to proceed — no auto-advance.
static void show_ready(shared_ptr<TreatmentState> state)
{
    const int W_NUM_Y      = 120;
    const int W_NUM_H      = 155;
    const int W_PCT_H      = 80;
    const int W_PCT_Y      = W_NUM_Y + W_NUM_H - W_PCT_H;  // bottom-aligned
    const int W_STATUS1_Y  = 285;
    const int W_STATUS2_Y  = 313;

    auto [container, _cum_lbl] = make_treatment_container(state, false);

    // ── Large "100" + "%" suffix at baseline, centred as a group ──────────
    auto num_label = make_shared<Label>("100");
    num_label->font(Font(120, Font::Weight::bold));
    num_label->color(Palette::ColorId::label_text, dt::kGreen);
    container->add(num_label);

    auto pct_sup = make_shared<Label>("%",
        Rect(0, W_PCT_Y, 90, W_PCT_H),
        AlignFlag::bottom | AlignFlag::left);
    pct_sup->font(Font(56, Font::Weight::bold));
    pct_sup->color(Palette::ColorId::label_text, dt::kGreen);
    container->add(pct_sup);

    center_pct_group(100, num_label, pct_sup, W_NUM_Y, W_NUM_H, W_PCT_Y);

    // ── "Ready" / "for Treatment" ──────────────────────────────────────────
    auto status1 = make_shared<Label>("Ready",
        Rect(0, W_STATUS1_Y, dt::SCREEN_W, 28));
    status1->font(Font(20, Font::Weight::normal));
    status1->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status1);

    auto status2 = make_shared<Label>("for Treatment",
        Rect(0, W_STATUS2_Y, dt::SCREEN_W, 28));
    status2->font(Font(20, Font::Weight::normal));
    status2->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status2);

    // ── "Begin Treatment" button (centered, green filled) ─────────────────
    auto btn_begin = ui::create_filled_button("Begin\nTreatment",
        Rect((dt::SCREEN_W - 260) / 2, BTN_Y, 260, BTN_H),
        [=]() {
            show_position_tip(state);
        });
    container->add(btn_begin);

    state->callbacks.on_show_screen(container);
}

// ── POSITION TIP SCREEN ────────────────────────────────────────────────────
// Figma Group 209: Logo, DEMO MODE, cumulative time header, countdown,
// status text, Pause/End buttons. The cumulative header was previously
// suppressed which made the user feel the timer "disappeared" between
// cycles — keep it visible so the running total stays anchored.
static void show_position_tip(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl2] = make_treatment_container(state, true);

    bool is_reposition = state->cycles_completed > 0;
    string title = is_reposition
        ? "Reposition the Applicator Tip"
        : "Position the Applicator Tip";

    // Large countdown display (Figma: y=60, 64px)
    auto countdown_val = make_shared<int>(state->config.position_tip_seconds);
    auto countdown_label = make_shared<Label>(
        TreatmentState::format_time(*countdown_val),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(dt::fontHuge());
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text below countdown (Figma: y=130)
    auto status = make_shared<Label>(title,
        Rect(0, STATUS_Y, dt::SCREEN_W, 30));
    status->font(dt::fontBody());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Pause / End buttons (Figma: y=194, left=21, right=290)
    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    // Countdown timer
    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    state->active_timer = timer;

    weak_ptr<Label> w_label = countdown_label;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*countdown_val)--;
        if (auto lb = w_label.lock())
            lb->text(TreatmentState::format_time(*countdown_val));

        if (*countdown_val <= 0) {
            timer->cancel();
            show_treatment_active(state);
        }
    });
    timer->start();
}

// ── TREATMENT ACTIVE SCREEN ────────────────────────────────────────────────
// ── TREATMENT ACTIVE SCREEN ────────────────────────────────────────────────
// Figma Group 150: Logo, DEMO MODE, cumulative time (header), countdown,
// status text, segmented dots, Pause/End buttons.
static void show_treatment_active(shared_ptr<TreatmentState> state)
{
    auto [container, cum_time_lbl] = make_treatment_container(state, true);

    // Large countdown (Figma: y=60, 64px)
    int cycle_remaining = state->config.cycle_seconds;
    int remaining_total = state->config.total_target_seconds - state->cumulative_seconds;
    cycle_remaining = min(cycle_remaining, remaining_total);

    auto remaining = make_shared<int>(cycle_remaining);
    auto countdown_label = make_shared<Label>(
        to_string(*remaining),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(dt::fontHuge());
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text below countdown (Figma: y=130)
    auto status = make_shared<Label>("Treatment started",
        Rect(0, STATUS_Y, dt::SCREEN_W, 30));
    status->font(dt::fontBody());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Segmented progress dots (Figma: y=154)
    add_segmented_progress(container, state);

    // Pause / End buttons (Figma: y=194, left=21, right=290)
    auto timer_ref = make_shared<shared_ptr<PeriodicTimer>>(nullptr);

    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    // End Treatment: green fill + white text (Figma green/white palette)
    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_GREEN_FILLED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    // Active treatment timer — 1 second ticks
    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    *timer_ref = timer;
    state->active_timer = timer;

    weak_ptr<Label> w_countdown = countdown_label;
    weak_ptr<Label> w_cum_time = cum_time_lbl;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock())
            lb->text(to_string(*remaining));

        // Update cumulative time dynamically
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));

        // Transition to green near-end screen at threshold
        if (*remaining <= state->config.nearly_finished_threshold && *remaining > 0) {
            timer->cancel();
            show_treatment_nearly_done(state, *remaining);
            return;
        }

        // Check if treatment target reached (e.g. last cycle short)
        if (state->is_complete()) {
            timer->cancel();
            state->cycles_completed++;
            show_treatment_completed(state, false);
            return;
        }

        // Check if cycle ended
        if (*remaining <= 0) {
            timer->cancel();
            state->cycles_completed++;
            state->current_cycle++;
            show_position_tip(state);
        }
    });
    timer->start();
}

// ── TREATMENT NEARLY DONE SCREEN ───────────────────────────────────────────
// Figma 67:578: white interior framed by a green border band. Same dark
// text and layout as the normal cycle screen — the only "urgency" cue is
// the green frame + dashed bar in place of segmented dots. Transitions to
// completed (if treatment done) or position_tip (if next cycle).
static void show_treatment_nearly_done(shared_ptr<TreatmentState> state, int remaining_seconds)
{
    auto [container, cum_time_lbl] = make_treatment_container(state, true, true);

    auto remaining = make_shared<int>(remaining_seconds);

    // Large countdown — dark text (the frame already carries the green cue)
    auto countdown_label = make_shared<Label>(
        to_string(*remaining),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    countdown_label->font(dt::fontHuge());
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Status text — dark
    auto status = make_shared<Label>("Treatment nearly finished",
        Rect(0, STATUS_Y, dt::SCREEN_W, 30));
    status->font(dt::fontBody());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Dashed progress bar replaces the round dots. Green dashes represent
    // time REMAINING in this final stretch — full at the start, draining
    // from right to left as the seconds tick down.
    const int total_dashes = 30;
    const int threshold    = state->config.nearly_finished_threshold;
    int filled = (threshold > 0)
        ? (*remaining * total_dashes) / threshold
        : 0;
    if (filled < 0) filled = 0;
    if (filled > total_dashes) filled = total_dashes;
    auto dash_bar = make_shared<DashedProgressBar>(
        Rect(60, DOTS_Y - 4, dt::SCREEN_W - 120, 12),
        total_dashes, filled);
    container->add(dash_bar);

    // Buttons — standard outlined (dark text on white bg)
    auto timer_ref = make_shared<shared_ptr<PeriodicTimer>>(nullptr);

    auto btn_pause = make_action_button("Pause", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);

    auto timer = make_shared<PeriodicTimer>(chrono::seconds(1));
    *timer_ref = timer;
    state->active_timer = timer;

    weak_ptr<Label>             w_countdown = countdown_label;
    weak_ptr<Label>             w_cum_time  = cum_time_lbl;
    weak_ptr<DashedProgressBar> w_dash      = dash_bar;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock())
            lb->text(to_string(*remaining));
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));
        if (auto db = w_dash.lock()) {
            db->set_filled((threshold > 0)
                ? (*remaining * total_dashes) / threshold
                : 0);
        }

        // Treatment target reached?
        if (state->is_complete()) {
            timer->cancel();
            state->cycles_completed++;
            show_treatment_completed(state, false);
            return;
        }

        // Cycle ended (multi-cycle scenario)
        if (*remaining <= 0) {
            timer->cancel();
            state->cycles_completed++;
            state->current_cycle++;
            show_position_tip(state);
        }
    });
    timer->start();
}

// ── TREATMENT PAUSED SCREEN ────────────────────────────────────────────────
// Figma Group 160: Logo, DEMO MODE, cumulative time (header), large paused time,
// status text, tip message, Resume/End buttons. No segmented dots.
static void show_treatment_paused(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl3] = make_treatment_container(state, true);
    state->is_paused = true;

    // Large paused time display (Figma: y=60, 64px)
    auto time_display = make_shared<Label>(
        TreatmentState::format_time(state->cumulative_seconds),
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    time_display->font(dt::fontHuge());
    time_display->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(time_display);

    // "Treatment Paused" text below (Figma: y=130)
    auto status = make_shared<Label>("Treatment Paused",
        Rect(0, STATUS_Y, dt::SCREEN_W, 25));
    status->font(dt::fontBody());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Tip message (Figma: y=155, cyan text with decorative brackets)
    auto tip = make_shared<Label>(
        "Tip: Keep pauses short to quickly rewarm and get back to treatment faster!",
        Rect(60, STATUS_Y + 30, dt::SCREEN_W - 120, 60));
    tip->font(dt::fontBody());
    tip->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(tip);

    // Resume / End buttons (Figma: y=198, Resume=filled left, End=outlined right)
    auto btn_resume = make_action_button("Resume", "Treatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        BTN_CYAN_FILLED,
        [=]() {
            state->is_paused = false;
            show_warming(state);
        });
    container->add(btn_resume);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_OUTLINED,
        [=]() {
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);
}

// ── END CONFIRMATION DIALOG ────────────────────────────────────────────────
// Figma Group 51: Logo, DEMO MODE, card with "End Treatment" title + question,
// Cancel/End buttons at bottom. No cumulative, no dots.
static void show_end_confirmation(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl4] = make_treatment_container(state, false);

    // Card frame (Figma: Rectangle 38 at y=53, 232×116 → scaled ~430×215)
    const int card_w = 440;
    const int card_h = 220;
    const int card_x = (dt::SCREEN_W - card_w) / 2;
    const int card_y = CONTENT_Y - 15;
    auto card = make_shared<Frame>(Rect(card_x, card_y, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kGrayBg);
    card->border_radius(dt::RADIUS_LG);
    card->border(0);
    container->add(card);

    // "End Treatment" title (Figma: y=80, 16px bold)
    auto title = make_shared<Label>("End Treatment",
        Rect(20, 15, card_w - 40, 30));
    title->font(dt::fontTitle());
    title->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(title);

    // Question text (Figma: y=106)
    auto question = make_shared<Label>(
        "Are you sure you want to\nend treatment?",
        Rect(20, 55, card_w - 40, 60));
    question->font(dt::fontBody());
    question->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(question);

    // Cancel (left, outlined) / End Treatment (right, filled)
    // Figma: Cancel at left (21,194), End at right (297,194)
    auto btn_cancel = ui::create_outlined_button("Cancel",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        [=]() {
            if (state->is_paused)
                show_treatment_paused(state);
            else
                show_treatment_active(state);
        });
    container->add(btn_cancel);

    auto btn_end = make_action_button("End", "Treatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
        BTN_CYAN_FILLED,
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_completed(state, true);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);
}

// ── TREATMENT COMPLETED / ENDED SCREEN ─────────────────────────────────────
// Figma Group 152/174: Logo, DEMO MODE, cumulative time (header), large "0",
// "Treatment Completed", segmented dots (all filled), checkmark icon.
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early)
{
    string title = early ? "Treatment Ended" : "Treatment Completed";
    auto [container, _cum_lbl5] = make_treatment_container(state, true);

    // Large final counter (Figma: "0" at y=60, 64px)
    auto icon_label = make_shared<Label>(early ? "✕" : "0",
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    icon_label->font(dt::fontHuge());
    icon_label->color(Palette::ColorId::label_text,
        early ? dt::kOrange : dt::kTextPrimary);
    container->add(icon_label);

    // Title text (Figma: y=130, 16px bold)
    auto title_lbl = make_shared<Label>(title,
        Rect(0, STATUS_Y, dt::SCREEN_W, 30));
    title_lbl->font(Font(dt::FONT_BODY, Font::Weight::bold));
    title_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title_lbl);

    // Segmented dots (all filled for completed)
    add_segmented_progress(container, state);

    // Circle checkmark icon (green ring + green ✓ inside)
    if (!early) {
        const int icon_sz = 72;
        const int icon_x  = (dt::SCREEN_W - icon_sz) / 2;
        const int icon_y  = DOTS_Y + 28;

        auto circle = make_shared<Frame>(Rect(icon_x, icon_y, icon_sz, icon_sz));
        circle->fill_flags({Theme::FillFlag::blend});
        circle->color(Palette::ColorId::bg, dt::kBgWhite);
        circle->color(Palette::ColorId::border, dt::kGreen);
        circle->border(4);
        circle->border_radius(icon_sz / 2);
        container->add(circle);

        // ✓ inside the circle (positioned relative to circle)
        auto check_lbl = make_shared<Label>("✓",
            Rect(0, 0, icon_sz, icon_sz));
        check_lbl->font(Font(38, Font::Weight::bold));
        check_lbl->color(Palette::ColorId::label_text, dt::kGreen);
        circle->add(check_lbl);
    }

    // Back to Home button at bottom
    auto go_home = [=]() {
        if (state->active_timer) state->active_timer->cancel();
        if (early && state->callbacks.on_treatment_ended_early)
            state->callbacks.on_treatment_ended_early();
        else if (!early && state->callbacks.on_treatment_completed)
            state->callbacks.on_treatment_completed();
    };

    auto btn_home = ui::create_filled_button("Back to Home",
        Rect((dt::SCREEN_W - BTN_W) / 2, BTN_Y, BTN_W, BTN_H),
        go_home);
    container->add(btn_home);

    state->callbacks.on_show_screen(container);

    // Auto-return to home after a few seconds (Figma design note)
    auto auto_timer = make_shared<PeriodicTimer>(chrono::seconds(5));
    state->active_timer = auto_timer;
    auto_timer->on_timeout([=]() {
        auto_timer->cancel();
        if (!*state->alive) return;   // user already left — don't pop back
        go_home();
    });
    auto_timer->start();
}
