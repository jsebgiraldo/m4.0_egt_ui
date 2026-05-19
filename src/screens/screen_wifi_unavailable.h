#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_UNAVAILABLE — "Not Connected" screen (Figma Wi-Fi Not Connected state 1).
///
/// Layout: orange "Wi-Fi Network not found" banner at the top, a white card
/// with the "Operate without WiFi · OVERRIDE MODE" affordance in the middle,
/// and a bottom strip with [Retry WiFi] + [Setting] icon-buttons.
///
/// Callbacks:
///   on_retry_wifi  — user wants to scan again (bounces to wifi_settings)
///   on_settings    — open the device Settings menu
///   on_override    — confirm "Operate in Override Mode" (the override flow)
///   on_back        — return to the previous screen (the WiFi list)
std::shared_ptr<egt::Widget> create_wifi_unavailable_screen(
    std::function<void()> on_retry_wifi,
    std::function<void()> on_settings,
    std::function<void()> on_override,
    std::function<void()> on_back);
