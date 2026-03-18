#pragma once
/// Shared UI components matching Figma "Jason M4.0" design.

#include <egt/ui>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace ui {

// ── Header Bar ──────────────────────────────────────────────────────────────
/// Creates consistent header: logo (top-left) + title (center) + optional demo badge.
std::shared_ptr<egt::Frame> create_header_bar(
    const std::string& title,
    bool demo_mode = false);

// ── Buttons ─────────────────────────────────────────────────────────────────
/// Outlined button (white bg, gray text) — like Figma "bt new".
std::shared_ptr<egt::Button> create_outlined_button(
    const std::string& text,
    const egt::Rect& rect,
    std::function<void()> on_click);

/// Filled button (cyan bg, white text) — like Figma "bt new over".
std::shared_ptr<egt::Button> create_filled_button(
    const std::string& text,
    const egt::Rect& rect,
    std::function<void()> on_click);

/// Small text button (for secondary actions like "Setting", "Demo Mode").
std::shared_ptr<egt::Button> create_text_button(
    const std::string& text,
    const egt::Rect& rect,
    std::function<void()> on_click);

// ── Segmented Progress Bar ──────────────────────────────────────────────────
/// 6-segment dotted progress bar: green = completed, gray = remaining.
/// Returns the frame. Call update_segmented_progress() to change filled count.
std::shared_ptr<egt::Frame> create_segmented_progress(
    int x, int y, int total_segments = 6);

void update_segmented_progress(
    std::shared_ptr<egt::Frame> bar,
    int filled_segments,
    int total_segments = 6);

// ── Linear Progress Bar ────────────────────────────────────────────────────
/// Horizontal progress bar (green fill on gray track).
std::shared_ptr<egt::Frame> create_linear_progress_bar(
    int x, int y, int width, int height = 12);

void update_linear_progress(
    std::shared_ptr<egt::Frame> bar,
    float percent);  // 0.0 – 100.0

// ── Cumulative Time Footer ─────────────────────────────────────────────────
/// Line separator + time display + "Cumulative treatment time" label.
struct CumulativeTimeFooter {
    std::shared_ptr<egt::Frame> frame;
    std::shared_ptr<egt::Label> time_label;
};

CumulativeTimeFooter create_cumulative_time_footer(
    int x, int y, int width);

// ── Demo Mode Badge ────────────────────────────────────────────────────────
/// Cyan "DEMO MODE" text + leave (house) button.
struct DemoModeBadge {
    std::shared_ptr<egt::Frame> frame;
    std::shared_ptr<egt::Button> leave_btn;
};

DemoModeBadge create_demo_mode_badge(
    int x, int y,
    std::function<void()> on_leave);

// ── Error Overlay ──────────────────────────────────────────────────────────
enum class ErrorSeverity { Informational, Warning, CriticalFault };

std::shared_ptr<egt::Frame> create_error_overlay(
    ErrorSeverity severity,
    const std::string& title,
    const std::string& message,
    const std::string& error_code = "",
    std::function<void()> on_dismiss = nullptr);

// ── Logo ────────────────────────────────────────────────────────────────────
std::shared_ptr<egt::Widget> create_logo(int x, int y, int w, int h);

} // namespace ui
