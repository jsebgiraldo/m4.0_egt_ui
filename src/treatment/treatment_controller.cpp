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

// ── Common helpers ──────────────────────────────────────────────────────────
static TreatmentScreen make_treatment_container(
    shared_ptr<TreatmentState> state,
    const string& status_text)
{
    auto container = make_shared<Frame>(Rect(0, 0, dt::SCREEN_W, dt::SCREEN_H));
    container->color(Palette::ColorId::bg, dt::kBgWhite);

    // Logo (top-left, small)
    auto logo = ui::create_logo(15, 10, 80, 50);
    container->add(logo);

    // Status text (top-center)
    auto status = make_shared<Label>(status_text,
        Rect(100, 15, dt::SCREEN_W - 200, 40));
    status->align(AlignFlag::center);
    status->font(dt::fontSubtitle());
    status->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(status);

    // Demo mode badge (top-right)
    if (state->config.demo_mode) {
        auto badge = ui::create_demo_mode_badge(
            dt::SCREEN_W - 220, 5, state->callbacks.on_leave_to_home);
        container->add(badge.frame);
    }

    // Segmented progress bar
    int seg_total = state->total_cycles();
    int seg_filled = state->cycles_completed;
    int seg_display = min(seg_total, dt::SEGMENT_COUNT);
    // Map cycles to 6-segment display
    int filled_display = 0;
    if (seg_total > 0)
        filled_display = min(seg_display,
            (int)round((double)seg_filled / seg_total * seg_display));

    auto seg_bar = ui::create_segmented_progress(
        (dt::SCREEN_W - (seg_display * (dt::SEGMENT_W + dt::SEGMENT_GAP) - dt::SEGMENT_GAP)) / 2,
        dt::HEADER_H + 5,
        seg_display);
    ui::update_segmented_progress(seg_bar, filled_display, seg_display);
    container->add(seg_bar);

    // Cumulative time footer
    auto footer = ui::create_cumulative_time_footer(
        (dt::SCREEN_W - 400) / 2,
        dt::SCREEN_H - 65,
        400);
    footer.time_label->text(TreatmentState::format_time(state->cumulative_seconds));
    container->add(footer.frame);

    return {container, footer.time_label};
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
static void show_warming(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl] = make_treatment_container(state, "Warming up");

    // Large percentage display
    auto pct_label = make_shared<Label>("0%",
        Rect(0, dt::HEADER_H + 40, dt::SCREEN_W, 120));
    pct_label->align(AlignFlag::center);
    pct_label->font(dt::fontHuge());
    pct_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(pct_label);

    // Linear progress bar
    const int bar_w = 500;
    auto progress_bar = ui::create_linear_progress_bar(
        (dt::SCREEN_W - bar_w) / 2,
        dt::HEADER_H + 180,
        bar_w, 16);
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
static void show_position_tip(shared_ptr<TreatmentState> state)
{
    bool is_reposition = state->cycles_completed > 0;
    string title = is_reposition
        ? "Reposition the Applicator Tip"
        : "Position the Applicator Tip";

    auto [container, _cum_lbl2] = make_treatment_container(state, title);

    // Large countdown display
    auto countdown_val = make_shared<int>(state->config.position_tip_seconds);
    auto countdown_label = make_shared<Label>(
        TreatmentState::format_time(*countdown_val),
        Rect(0, dt::HEADER_H + 50, dt::SCREEN_W, 140));
    countdown_label->align(AlignFlag::center);
    countdown_label->font(dt::fontHuge());
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Pause / End buttons
    const int btn_y = dt::SCREEN_H - 140;
    const int btn_w = 180;
    const int btn_h = 70;
    const int gap = 40;
    const int total_w = btn_w * 2 + gap;
    const int btn_x = (dt::SCREEN_W - total_w) / 2;

    auto btn_pause = ui::create_outlined_button("Pause",
        Rect(btn_x, btn_y, btn_w, btn_h),
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = ui::create_outlined_button("End",
        Rect(btn_x + btn_w + gap, btn_y, btn_w, btn_h),
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
static void show_treatment_active(shared_ptr<TreatmentState> state)
{
    auto [container, cum_time_lbl] = make_treatment_container(state, "Treatment started");

    // Large countdown (seconds remaining in this cycle)
    int cycle_remaining = state->config.cycle_seconds;
    // If we'd exceed target, clamp
    int remaining_total = state->config.total_target_seconds - state->cumulative_seconds;
    cycle_remaining = min(cycle_remaining, remaining_total);

    auto remaining = make_shared<int>(cycle_remaining);
    auto countdown_label = make_shared<Label>(
        to_string(*remaining),
        Rect(0, dt::HEADER_H + 40, dt::SCREEN_W, 140));
    countdown_label->align(AlignFlag::center);
    countdown_label->font(dt::fontHuge());
    countdown_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(countdown_label);

    // Pause / End buttons
    const int btn_y = dt::SCREEN_H - 140;
    const int btn_w = 180;
    const int btn_h = 70;
    const int gap = 40;
    const int total_w = btn_w * 2 + gap;
    const int btn_x = (dt::SCREEN_W - total_w) / 2;

    auto timer_ref = make_shared<shared_ptr<PeriodicTimer>>(nullptr);

    auto btn_pause = ui::create_outlined_button("Pause",
        Rect(btn_x, btn_y, btn_w, btn_h),
        [=]() {
            if (*timer_ref) (*timer_ref)->cancel();
            show_treatment_paused(state);
        });
    container->add(btn_pause);

    auto btn_end = ui::create_outlined_button("End",
        Rect(btn_x + btn_w + gap, btn_y, btn_w, btn_h),
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
static void show_treatment_paused(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl3] = make_treatment_container(state, "Treatment Paused");
    state->is_paused = true;

    // Cumulative time (large)
    auto time_display = make_shared<Label>(
        TreatmentState::format_time(state->cumulative_seconds),
        Rect(0, dt::HEADER_H + 50, dt::SCREEN_W, 120));
    time_display->align(AlignFlag::center);
    time_display->font(dt::fontHuge());
    time_display->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(time_display);

    // Tip message
    auto tip = make_shared<Label>(
        "Tip: Keep pauses short to quickly rewarm\nand get back to treatment faster!",
        Rect(60, dt::HEADER_H + 190, dt::SCREEN_W - 120, 60));
    tip->align(AlignFlag::center);
    tip->font(dt::fontSmall());
    tip->color(Palette::ColorId::label_text, dt::kAccentCyan);
    container->add(tip);

    // Resume / End buttons
    const int btn_y = dt::SCREEN_H - 140;
    const int btn_w = 180;
    const int btn_h = 70;
    const int gap = 40;
    const int total_w = btn_w * 2 + gap;
    const int btn_x = (dt::SCREEN_W - total_w) / 2;

    auto btn_resume = ui::create_filled_button("Resume",
        Rect(btn_x, btn_y, btn_w, btn_h),
        [=]() {
            state->is_paused = false;
            // Re-warm then continue
            show_warming(state);
        });
    container->add(btn_resume);

    auto btn_end = ui::create_outlined_button("End",
        Rect(btn_x + btn_w + gap, btn_y, btn_w, btn_h),
        [=]() {
            show_end_confirmation(state);
        });
    container->add(btn_end);

    state->callbacks.on_show_screen(container);
}

// ── END CONFIRMATION DIALOG ────────────────────────────────────────────────
static void show_end_confirmation(shared_ptr<TreatmentState> state)
{
    auto [container, _cum_lbl4] = make_treatment_container(state, "End Treatment");

    // Center card
    const int card_w = 440;
    const int card_h = 220;
    auto card = make_shared<Frame>(
        Rect((dt::SCREEN_W - card_w) / 2, (dt::SCREEN_H - card_h) / 2, card_w, card_h));
    card->fill_flags({Theme::FillFlag::blend});
    card->color(Palette::ColorId::bg, dt::kGrayBg);
    card->border_radius(dt::RADIUS_LG);
    card->border(0);
    container->add(card);

    // Question
    auto question = make_shared<Label>(
        "Are you sure you want to\nend treatment?",
        Rect(20, 20, card_w - 40, 80));
    question->align(AlignFlag::center);
    question->font(dt::fontSubtitle());
    question->color(Palette::ColorId::label_text, dt::kTextPrimary);
    card->add(question);

    // End / Cancel buttons
    const int btn_w = 180;
    const int btn_h = 60;
    const int gap = 20;

    auto btn_end = ui::create_filled_button("End Treatment",
        Rect((card_w - btn_w * 2 - gap) / 2, card_h - 90, btn_w, btn_h),
        [=]() {
            if (state->active_timer) state->active_timer->cancel();
            show_treatment_completed(state, true);
        });
    card->add(btn_end);

    auto btn_cancel = ui::create_outlined_button("Cancel",
        Rect((card_w + gap) / 2, card_h - 90, btn_w, btn_h),
        [=]() {
            // Go back to active or paused
            if (state->is_paused)
                show_treatment_paused(state);
            else
                show_treatment_active(state);
        });
    card->add(btn_cancel);

    state->callbacks.on_show_screen(container);
}

// ── TREATMENT COMPLETED / ENDED SCREEN ─────────────────────────────────────
static void show_treatment_completed(shared_ptr<TreatmentState> state, bool early)
{
    string title = early ? "Treatment Ended" : "Treatment Completed";
    auto [container, _cum_lbl5] = make_treatment_container(state, title);

    // Checkmark or end icon placeholder
    auto icon_label = make_shared<Label>(early ? "✕" : "✓",
        Rect(0, dt::HEADER_H + 30, dt::SCREEN_W, 80));
    icon_label->align(AlignFlag::center);
    icon_label->font(Font(60, Font::Weight::bold));
    icon_label->color(Palette::ColorId::label_text,
        early ? dt::kOrange : dt::kGreen);
    container->add(icon_label);

    // Title
    auto title_lbl = make_shared<Label>(title,
        Rect(0, dt::HEADER_H + 110, dt::SCREEN_W, 40));
    title_lbl->align(AlignFlag::center);
    title_lbl->font(dt::fontTitle());
    title_lbl->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(title_lbl);

    // Cumulative time (large)
    auto time_label = make_shared<Label>(
        TreatmentState::format_time(state->cumulative_seconds),
        Rect(0, dt::HEADER_H + 170, dt::SCREEN_W, 60));
    time_label->align(AlignFlag::center);
    time_label->font(dt::fontLarge());
    time_label->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(time_label);

    auto desc = make_shared<Label>("Cumulative treatment time",
        Rect(0, dt::HEADER_H + 230, dt::SCREEN_W, 25));
    desc->align(AlignFlag::center);
    desc->font(dt::fontSmall());
    desc->color(Palette::ColorId::label_text, dt::kTextPrimary);
    container->add(desc);

    // Back to Home button
    auto go_home = [=]() {
        if (state->active_timer) state->active_timer->cancel();
        if (early && state->callbacks.on_treatment_ended_early)
            state->callbacks.on_treatment_ended_early();
        else if (!early && state->callbacks.on_treatment_completed)
            state->callbacks.on_treatment_completed();
    };

    auto btn_home = ui::create_filled_button("Back to Home",
        Rect((dt::SCREEN_W - 200) / 2, dt::SCREEN_H - 100, 200, 60),
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
