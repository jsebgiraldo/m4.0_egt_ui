#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <vector>
#include "../wifi/wifi_backend.h"

/// WiFi Initialization screen (Figma: first boot screen).
/// Shows logo + spinner + "Connecting to WiFi" text.
/// Calls on_connected after simulated connection delay, or on_failed if WiFi unavailable.
/// on_failed receives the pre-scanned networks (if any) so the settings screen
/// can display them immediately without a redundant scan.
/// on_skip allows user to bypass WiFi and go straight to login.
std::shared_ptr<egt::Widget> create_wifi_init_screen(
    std::function<void()> on_connected,
    std::function<void(std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>>)> on_failed,
    std::function<void()> on_skip = nullptr);
