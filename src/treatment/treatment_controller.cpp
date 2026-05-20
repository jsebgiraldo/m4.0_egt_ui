#include "treatment_controller.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace egt;
using namespace std;

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
        const int dash_w = 12;
        const int dash_h = 6;
        const int small_gap = 3;
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

    // Process-limit warnings fire once each as cumulative time crosses the
    // 5-min and 1-min-remaining thresholds. Tracked here so they don't
    // re-fire when a screen rebuilds between cycles.
    bool first_warning_fired  = false;
    bool second_warning_fired = false;

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

    // How many total cycles to reach the process limit
    int total_cycles() const {
        if (config.cycle_seconds <= 0) return 1;
        return max(1, (config.process_limit_seconds + config.cycle_seconds - 1) / config.cycle_seconds);
    }

    // Has the process hit its hard time limit?
    bool is_complete() const {
        return cumulative_seconds >= config.process_limit_seconds;
    }
};

// Fire the process-limit advance-notice alerts (tone + LED) once each as
// cumulative time crosses the configured thresholds. Safe to call from any
// per-second tick — the state flags guarantee single-fire.
static void check_and_fire_warnings(shared_ptr<TreatmentState> state)
{
    const int remaining =
        state->config.process_limit_seconds - state->cumulative_seconds;

    if (!state->first_warning_fired &&
        remaining <= state->config.first_warning_remaining) {
        state->first_warning_fired = true;
        printf("[PROCESS] First warning — %d s remaining (1 tone, 1 LED flash)\n",
               remaining); fflush(stdout);
        if (state->callbacks.on_alert_tone)    state->callbacks.on_alert_tone(1);
        if (state->callbacks.on_tip_led_flash) state->callbacks.on_tip_led_flash(1);
    }
    if (!state->second_warning_fired &&
        remaining <= state->config.second_warning_remaining) {
        state->second_warning_fired = true;
        printf("[PROCESS] Second warning — %d s remaining (2 tones, 2 LED flashes)\n",
               remaining); fflush(stdout);
        if (state->callbacks.on_alert_tone)    state->callbacks.on_alert_tone(2);
        if (state->callbacks.on_tip_led_flash) state->callbacks.on_tip_led_flash(2);
    }
}

// Which warning window the current cumulative time falls in.
enum class WarnWindow { None, FiveMin, OneMin };
static WarnWindow current_warn_window(const shared_ptr<TreatmentState>& state)
{
    const int remaining =
        state->config.process_limit_seconds - state->cumulative_seconds;
    if (remaining <= state->config.second_warning_remaining) return WarnWindow::OneMin;
    if (remaining <= state->config.first_warning_remaining)  return WarnWindow::FiveMin;
    return WarnWindow::None;
}

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
        // Outer green band with rounded corners + inner white rectangle
        // (also rounded, slightly smaller radius) — gives the chunky
        // pill-frame look from Figma 67:578 without drawing it pixel by
        // pixel in a custom widget.
        const int bw = 14;
        auto green_outer = make_shared<Frame>(
            Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
        green_outer->fill_flags({Theme::FillFlag::blend});
        green_outer->color(Palette::ColorId::bg, dt::kGreen);
        green_outer->border(0);
        green_outer->border_radius(20);
        container->add(green_outer);

        auto white_inner = make_shared<Frame>(
            Rect(bw, bw, dt::SCREEN_W - 2 * bw, dt::SCREEN_H - 2 * bw));
        white_inner->fill_flags({Theme::FillFlag::blend});
        white_inner->color(Palette::ColorId::bg, dt::kBgWhite);
        white_inner->border(0);
        white_inner->border_radius(8);
        container->add(white_inner);
    }

    // Logo (top-left, full Figma size)
    auto logo = ui::create_logo(10, 5, dt::LOGO_W, dt::LOGO_H);
    container->add(logo);

    // Demo mode badge (top-right, vertical: DEMO MODE label + Exit below)
    if (state->config.demo_mode) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 180, 24, state->callbacks.on_leave_to_home);
        container->add(badge.frame);
    }

    shared_ptr<Label> cum_time_lbl;

    // Cumulative time header (ToDo master task 6): a two-line label
    // "CUMULATIVE / TREATMENT TIME" next to the big time value, the whole
    // pair centred horizontally on the screen and nudged lower than before
    // (it used to hug the very top). Layout is a centred group:
    //   [ right-aligned 2-line label ] gap [ big time value ]
    if (show_cumulative) {
        const int desc_w = 210, time_w = 120, gap = 14;
        const int group_w = desc_w + gap + time_w;
        const int group_x = (dt::SCREEN_W - group_w) / 2;  // centred
        const int hdr_y   = 28;                             // lower than before
        const int sep_y   = 86;

        // Two-line description, right-aligned so it reads tight against the
        // time value.
        auto desc1 = make_shared<Label>("CUMULATIVE",
            Rect(group_x, hdr_y, desc_w, 24),
            AlignFlag::right | AlignFlag::center_vertical);
        desc1->font(Font(15, Font::Weight::normal));
        desc1->color(Palette::ColorId::label_text, text_color);
        container->add(desc1);

        auto desc2 = make_shared<Label>("TREATMENT TIME",
            Rect(group_x, hdr_y + 22, desc_w, 24),
            AlignFlag::right | AlignFlag::center_vertical);
        desc2->font(Font(15, Font::Weight::normal));
        desc2->color(Palette::ColorId::label_text, text_color);
        container->add(desc2);

        // Big time value, vertically centred against the 2-line label.
        cum_time_lbl = make_shared<Label>(
            TreatmentState::format_time(state->cumulative_seconds),
            Rect(group_x + desc_w + gap, hdr_y, time_w, 46),
            AlignFlag::left | AlignFlag::center_vertical);
        cum_time_lbl->font(Font(28, Font::Weight::bold));
        cum_time_lbl->color(Palette::ColorId::label_text, text_color);
        container->add(cum_time_lbl);

        // Separator line under the group
        auto sep = make_shared<Frame>(Rect(CUM_LEFT_X, sep_y, CUM_WIDTH, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, sep_color);
        sep->border(0);
        container->add(sep);
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
    frame->border_flags({Theme::BorderFlag::drop_shadow});

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

// Overall treatment progress as a 0..1 fraction of the process time limit.
static float treatment_progress(const shared_ptr<TreatmentState>& state)
{
    if (state->config.process_limit_seconds <= 0) return 0.0f;
    return static_cast<float>(state->cumulative_seconds) /
           static_cast<float>(state->config.process_limit_seconds);
}

// Add the segmented progress dots and return the bar so callers can update
// it live. The fill tracks cumulative time vs the process limit (not the
// cycle count) so it advances smoothly throughout the session instead of
// staying empty for many short cycles.
static shared_ptr<Frame> add_segmented_progress(
    shared_ptr<Frame> container,
    shared_ptr<TreatmentState> state)
{
    const int seg_display = dt::SEGMENT_COUNT;
    auto seg_bar = ui::create_segmented_progress(
        (dt::SCREEN_W - (seg_display * (dt::SEGMENT_W + dt::SEGMENT_GAP) - dt::SEGMENT_GAP)) / 2,
        DOTS_Y,
        seg_display);
    ui::update_segmented_progress_fraction(seg_bar, treatment_progress(state));
    container->add(seg_bar);
    return seg_bar;
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
    const int W_BAR_Y      = 360;

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

    // ── Full green progress bar (100%) — carried over from Warming so the
    //    "ready" state still reads as fully warmed (ToDo master task 8).
    const int bar_w = 700;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2, W_BAR_Y, bar_w, 10);
    container->add(progress_bar);
    ui::update_linear_progress(progress_bar, 100.0f);

    // ── "Begin Treatment" button — flow accent (green real, blue demo) ────
    const Color accent = state->config.demo_mode ? dt::kAccentCyan : dt::kGreen;
    auto btn_begin = ui::create_filled_button("Begin\nTreatment",
        Rect((dt::SCREEN_W - 260) / 2, BTN_Y, 260, BTN_H),
        [=]() {
            show_position_tip(state);
        });
    btn_begin->color(Palette::ColorId::button_bg, accent);
    btn_begin->color(Palette::ColorId::border, accent);
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
    int remaining_total = state->config.process_limit_seconds - state->cumulative_seconds;
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

    // Segmented progress dots (Figma: y=154) — tracks cumulative time
    auto seg_bar = add_segmented_progress(container, state);

    // ── MVP process-limit warning banner ──────────────────────────────
    // Hidden until cumulative time enters a warning window; amber at the
    // 5-min mark, red at the 1-min mark, with a live "limit in M:SS"
    // countdown. Placeholder styling for client review — Figma hasn't
    // defined these notification screens yet.
    const int wb_w = 460, wb_h = 40;
    auto warn_banner = make_shared<Frame>(
        Rect((dt::SCREEN_W - wb_w) / 2, 336, wb_w, wb_h));
    warn_banner->fill_flags({Theme::FillFlag::blend});
    warn_banner->border(0);
    warn_banner->border_radius(dt::RADIUS_SM);
    warn_banner->hide();
    container->add(warn_banner);

    auto warn_lbl = make_shared<Label>("",
        Rect(0, 0, wb_w, wb_h), AlignFlag::center);
    warn_lbl->font(Font(18, Font::Weight::bold));
    warn_lbl->color(Palette::ColorId::label_text, dt::kWhite);
    warn_banner->add(warn_lbl);

    auto refresh_banner = [state](shared_ptr<Frame> banner,
                                  shared_ptr<Label> lbl) {
        const auto win = current_warn_window(state);
        if (win == WarnWindow::None) { banner->hide(); return; }
        const int rem = std::max(0,
            state->config.process_limit_seconds - state->cumulative_seconds);
        banner->color(Palette::ColorId::bg,
            win == WarnWindow::OneMin ? dt::kRed : dt::kOrange);
        lbl->text("Process limit in " + TreatmentState::format_time(rem));
        banner->show();
    };
    refresh_banner(warn_banner, warn_lbl);  // reflect current state on build

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
    weak_ptr<Frame> w_warn_banner = warn_banner;
    weak_ptr<Label> w_warn_lbl = warn_lbl;
    weak_ptr<Frame> w_seg_bar = seg_bar;

    timer->on_timeout([=]() {
        if (!*state->alive) { timer->cancel(); return; }
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock())
            lb->text(to_string(*remaining));

        // Update cumulative time dynamically
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));

        // Update overall progress dots (cumulative vs process limit)
        if (auto sb = w_seg_bar.lock())
            ui::update_segmented_progress_fraction(sb, treatment_progress(state));

        // Process-limit alerts (tone + LED) + banner refresh
        check_and_fire_warnings(state);
        if (auto wb = w_warn_banner.lock())
            if (auto wl = w_warn_lbl.lock())
                refresh_banner(wb, wl);

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
        Rect(60, DOTS_Y - 6, dt::SCREEN_W - 120, 16),
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

        // Process-limit alerts (tone + LED) can still cross here
        check_and_fire_warnings(state);

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

    // Hero icon: a big ring with ✓ (completed, green) or ✕ (ended early,
    // orange). This replaces the old layout where a redundant "0"/"✕"
    // number sat up top AND a separate checkmark circle collided with the
    // Back-to-Home button (ToDo master task 7). Now it's a single centred
    // focal icon with nothing overlapping below it.
    const Color ring_color = early ? dt::kOrange : dt::kGreen;
    const int icon_sz = 120;
    const int icon_x  = (dt::SCREEN_W - icon_sz) / 2;
    const int icon_y  = 120;

    auto circle = make_shared<Frame>(Rect(icon_x, icon_y, icon_sz, icon_sz));
    circle->fill_flags({Theme::FillFlag::blend});
    circle->color(Palette::ColorId::bg, dt::kBgWhite);
    circle->color(Palette::ColorId::border, ring_color);
    circle->border(6);
    circle->border_radius(icon_sz / 2);
    container->add(circle);

    auto glyph = make_shared<Label>(early ? "✕" : "✓",
        Rect(0, 0, icon_sz, icon_sz), AlignFlag::center);
    glyph->font(Font(64, Font::Weight::bold));
    glyph->color(Palette::ColorId::label_text, ring_color);
    circle->add(glyph);

    // Title text below the hero icon
    auto title_lbl = make_shared<Label>(title,
        Rect(0, icon_y + icon_sz + 18, dt::SCREEN_W, 32));
    title_lbl->font(Font(dt::FONT_TITLE, Font::Weight::bold));
    title_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title_lbl);

    // Segmented dots (all filled) only on the completed (not ended) screen
    if (!early)
        add_segmented_progress(container, state);

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
