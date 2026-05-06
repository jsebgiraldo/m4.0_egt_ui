#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

/// Error severity levels matching Figma design:
///   INFO    — yellow icon, Resume/Pause/End buttons
///   WARNING — orange icon, no treatment actions (device blocked)
///   CRITICAL — red icon, device shutdown, contact support
enum class ErrorLevel { INFO, WARNING, CRITICAL };

/// Generic error screen (Figma error frames 143:870..143:876).
/// Displays title, message, and actions based on severity.
std::shared_ptr<egt::Widget> create_error_screen(
    ErrorLevel level,
    const std::string& title,
    const std::string& message,
    std::function<void()> on_primary,        // Resume (INFO) or OK/Acknowledge (WARNING/CRITICAL)
    std::function<void()> on_secondary = nullptr); // Pause (INFO) or nullptr
