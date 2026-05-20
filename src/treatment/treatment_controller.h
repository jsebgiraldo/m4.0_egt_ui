#pragma once
/// Treatment state machine controller.
/// Manages the full flow: Warming → Position Tip → Active → Nearly Finished
///                        → Position (next cycle) → ... → Completed
///                        With Pause/Resume and End Confirmation.

#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

/// Configuration for a treatment session (from backend).
struct TreatmentConfig {
    int warming_seconds       = 15;     // time to warm up (configurable)
    int position_tip_seconds  = 5;      // countdown to position applicator
    int cycle_seconds         = 30;     // seconds per treatment cycle (e.g. 25-30)

    // Hard limit on total process time (factory adjustable). The process
    // session is capped here; cycles keep running until cumulative time
    // reaches this. Spec example: 45 min.
    int process_limit_seconds = 2700;   // 45 minutes

    // Advance-notice warnings — seconds REMAINING when each fires. The
    // device alerts the technician that the process limit is approaching.
    //   first  → 5 min remaining (40 min elapsed): 1 tone + 1 LED flash
    //   second → 1 min remaining (44 min elapsed): 2 tones + 2 LED flashes
    int first_warning_remaining  = 300; // 5 minutes
    int second_warning_remaining = 60;  // 1 minute

    int nearly_finished_threshold = 5;  // last N seconds of a cycle trigger "nearly finished"
    bool demo_mode            = false;
};

/// Callbacks for the treatment flow to communicate with app.
struct TreatmentCallbacks {
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen;
    std::function<void()> on_treatment_completed;
    std::function<void()> on_treatment_ended_early;
    std::function<void()> on_leave_to_home;  // demo mode leave button

    // Hardware alerts for the process-limit warnings. count = number of
    // tones / flashes (1 at the 5-min warning, 2 at the 1-min warning).
    // Stubbed in the simulator; wired to firmware on the device.
    std::function<void(int count)> on_alert_tone;
    std::function<void(int count)> on_tip_led_flash;
};

/// Start the full treatment flow.
void start_treatment_flow(
    const TreatmentConfig& config,
    const TreatmentCallbacks& callbacks);
