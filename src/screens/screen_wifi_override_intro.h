#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_OVERRIDE_INTRO — simplified prompt before the override-password
/// keypad (Figma node 134:1421, S5-no3).
///
/// Sits between WiFi Unavailable and the password keypad. Shows the orange
/// "remains unavailable" banner, the short two-line explanation and one big
/// cyan "Enter Override password" button. The bottom row (Back / Retry WiFi /
/// Setting) mirrors WiFi Unavailable so the user can bail out at any point.
///
/// Callbacks:
///   on_enter_override — tap the cyan button → open password keypad
///   on_back           — return to the previous screen
///   on_retry_wifi     — bounce to the WiFi list to scan again
///   on_settings       — open the device Settings menu
std::shared_ptr<egt::Widget> create_wifi_override_intro_screen(
    std::function<void()> on_enter_override,
    std::function<void()> on_back,
    std::function<void()> on_retry_wifi,
    std::function<void()> on_settings);
