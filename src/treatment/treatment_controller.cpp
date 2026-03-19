#include "treatment_controller.h"
#include "../ui/components.h"
#include "../ui/design_tokens.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace egt;
using namespace std;

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
static void show_position_tip(shared_ptr<TreatmentState> state);
static void show_treatment_active(shared_ptr<TreatmentState> state);
static void show_treatment_paused(shared_ptr<TreatmentState> state);
static void show_end_confirmation(shared_ptr<TreatmentState> state);
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early);

// ── Container result with dynamic labels ────────────────────────────────────
struct TreatmentScreen {
    shared_ptr<Frame> container;
    shared_ptr<Label> cumulative_time_label;  // for dynamic updates
};

// ── Figma-matched layout constants (432×261 → 800×480) ─────────────────────
// Cumulative time header area (Figma: Group 159 at y=23, line at y=56)
static constexpr int CUM_TIME_Y     = 15;   // y for time labels
static constexpr int CUM_SEP_Y      = 55;   // y for separator line
static constexpr int CUM_LEFT_X     = 110;  // x start (right of logo)
static constexpr int CUM_WIDTH      = 470;  // width of cumulative area

// Main content area
static constexpr int CONTENT_Y      = 75;   // y for large number/percentage
static constexpr int CONTENT_H      = 120;  // height of large number area
static constexpr int STATUS_Y       = 210;  // y for status text below countdown
static constexpr int DOTS_Y         = 260;  // y for segmented progress dots
static constexpr int BTN_Y          = 345;  // y for bottom buttons
static constexpr int BTN_W          = 220;  // button width
static constexpr int BTN_H          = 80;   // button height
static constexpr int BTN_LEFT_X     = 40;   // left button x
static constexpr int BTN_RIGHT_X    = 540;  // right button x

// ── Common helpers ──────────────────────────────────────────────────────────
static TreatmentScreen make_treatment_container(
    shared_ptr<TreatmentState> state,
    bool show_cumulative)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->fill_flags({Theme::FillFlag::blend});
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo (top-left, small)
    auto logo = ui::create_logo(15, 10, 80, 50);
    container->add(logo);

    // Demo mode badge (top-right)
    if (state->config.demo_mode) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 220, 5, state->callbacks.on_leave_to_home);
        container->add(badge.frame);
    }

    shared_ptr<Label> cum_time_lbl;

    // Cumulative time in header area (Figma: between logo and badge)
    if (show_cumulative) {
        // Separator line
        auto sep = make_shared<Frame>(Rect(CUM_LEFT_X, CUM_SEP_Y, CUM_WIDTH, 1));
        sep->fill_flags({Theme::FillFlag::blend});
        sep->color(Palette::ColorId::bg, dt::kGrayLight);
        sep->border(0);
        container->add(sep);

        // Time value (right side)
        cum_time_lbl = make_shared<Label>(
            TreatmentState::format_time(state->cumulative_seconds),
            Rect(CUM_LEFT_X + CUM_WIDTH / 2, CUM_TIME_Y, CUM_WIDTH / 2, 35),
            AlignFlag::right);
        cum_time_lbl->font(Font(24, Font::Weight::bold));
        cum_time_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(cum_time_lbl);

        // Description (left side)
        auto desc = make_shared<Label>("Cumulative treatment time",
            Rect(CUM_LEFT_X, CUM_TIME_Y + 3, CUM_WIDTH / 2, 30),
            AlignFlag::left);
        desc->font(dt::fontSmall());
        desc->color(Palette::ColorId::label_text, dt::kTextPrimary);
        container->add(desc);
    }

    return {container, cum_time_lbl};
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

    show_warming(state);
}

// ── WARMING SCREEN ──────────────────────────────────────────────────────────
// Figma Group 47: Logo, DEMO MODE, Spinner with %, status text, progress bar.
// No cumulative time, no segmented dots, no buttons.
static void show_warming(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl] = make_treatment_container(state, false);

    // Large percentage display (Figma: y=60, 64px)
    auto pct_label = make_shared<Label>("0%",
        Rect(0, CONTENT_Y, dt::SCREEN_W, CONTENT_H));
    pct_label->font(dt::fontHuge());
    pct_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(pct_label);

    // "Warming up for Treatment" text below percentage (Figma: y=136)
    auto status = make_shared<Label>("Warming up for Treatment",
        Rect(0, STATUS_Y, dt::SCREEN_W, 30));
    status->font(dt::fontBody());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Linear progress bar near bottom (Figma: y=190/261=73% → ~350)
    const int bar_w = 700;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2,
        BTN_Y,
        bar_w, 10);
    container->add(progress_bar);

    state->callbacks.on_show_screen(container);

    // Animate warming over configured seconds
    auto progress_val = make_shared<float>(0.0f);
    auto elapsed_ms = make_shared<int>(0);
    const int total_ms = state->config.warming_seconds * 1000;

    auto timer = make_shared<PeriodicTimer>(chrono::milliseconds(50));
    state->active_timer = timer;

    weak_ptr<Label> w_pct = pct_label;
    weak_ptr<Frame> w_bar = progress_bar;

    timer->on_timeout([=]() {
        *elapsed_ms += 50;
        *progress_val = min(100.0f, (*elapsed_ms * 100.0f) / total_ms);

        if (auto lb = w_pct.lock())
            lb->text(to_string((int)*progress_val) + "%");
        if (auto bar = w_bar.lock())
            ui::update_linear_progress(bar, *progress_val);

        if (*elapsed_ms >= total_ms) {
            timer->cancel();
            show_position_tip(state);
        }
    });
    timer->start();
}

// ── POSITION TIP SCREEN ────────────────────────────────────────────────────
// Figma Group 209: Logo, DEMO MODE, countdown, status text, Pause/End buttons.
// No cumulative time, no segmented dots.
static void show_position_tip(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl2] = make_treatment_container(state, false);

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
    auto btn_pause = ui::create_outlined_button("Pause\nTreatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = ui::create_outlined_button("End\nTreatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
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

    auto btn_pause = ui::create_outlined_button("Pause\nTreatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = ui::create_outlined_button("End\nTreatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
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
        (*remaining)--;
        state->cumulative_seconds++;

        if (auto lb = w_countdown.lock()) {
            lb->text(to_string(*remaining));
            // Color change near end
            if (*remaining <= state->config.nearly_finished_threshold)
                lb->color(Palette::ColorId::label_text, dt::kOrange);
        }

        // Update cumulative time footer dynamically
        if (auto ct = w_cum_time.lock())
            ct->text(TreatmentState::format_time(state->cumulative_seconds));

        // Check if treatment target reached
        if (state->is_complete()) {
            timer->cancel();
            state->cycles_completed++;
            // Hold at zero for 3 seconds before transitioning (Figma design note)
            auto hold_timer = make_shared<PeriodicTimer>(chrono::seconds(3));
            state->active_timer = hold_timer;
            hold_timer->on_timeout([=]() {
                hold_timer->cancel();
                show_treatment_completed(state, false);
            });
            hold_timer->start();
            return;
        }

        // Check if cycle ended
        if (*remaining <= 0) {
            timer->cancel();
            state->cycles_completed++;
            state->current_cycle++;
            // Next cycle: reposition tip
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
        Rect(60, STATUS_Y + 30, dt::SCREEN_W - 120, 50));
    tip->font(dt::fontSmall());
    tip->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(tip);

    // Resume / End buttons (Figma: y=198, Resume=filled left, End=outlined right)
    auto btn_resume = ui::create_filled_button("Resume\nTreatment",
        Rect(BTN_LEFT_X, BTN_Y, BTN_W, BTN_H),
        [=]() {
            state->is_paused = false;
            show_warming(state);
        });
    container->add(btn_resume);

    auto btn_end = ui::create_outlined_button("End\nTreatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
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

    auto btn_end = ui::create_filled_button("End\nTreatment",
        Rect(BTN_RIGHT_X, BTN_Y, BTN_W, BTN_H),
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

    // Checkmark below dots (Figma: y=176, 35×35 → ~65×65)
    if (!early) {
        auto check = make_shared<Label>("✓",
            Rect(0, DOTS_Y + 25, dt::SCREEN_W, 50));
        check->font(Font(36, Font::Weight::bold));
        check->color(Palette::ColorId::label_text, dt::kGreen);
        container->add(check);
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
        go_home();
    });
    auto_timer->start();
}
