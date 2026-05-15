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
    int total_target_seconds  = 1800;   // 30 minutes total cumulative target
    int nearly_finished_threshold = 5;  // last N seconds of a cycle trigger "nearly finished"
    bool demo_mode            = false;
};

/// Callbacks for the treatment flow to communicate with app.
struct TreatmentCallbacks {
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen;
    std::function<void()> on_treatment_completed;
    std::function<void()> on_treatment_ended_early;
    std::function<void()> on_leave_to_home;  // demo mode leave button
};

/// Start the full treatment flow.
void start_treatment_flow(
    const TreatmentConfig& config,
    const TreatmentCallbacks& callbacks);
