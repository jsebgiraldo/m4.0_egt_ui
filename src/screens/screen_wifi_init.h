#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WiFi Initialization screen (Figma: first boot screen).
/// Shows logo + spinner + "Connecting to WiFi" text.
/// Calls on_connected after simulated connection delay, or on_failed if WiFi unavailable.
std::shared_ptr<egt::Widget> create_wifi_init_screen(
    std::function<void()> on_connected,
    std::function<void()> on_failed);
