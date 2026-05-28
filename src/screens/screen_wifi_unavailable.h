#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_UNAVAILABLE — "Not Connected" screen (Figma node 2079:2300, M4-19).
///
/// Single source for both states of the same Figma screen:
///   - WITHOUT the explanatory dark popup (initial "no WiFi" view)
///   - WITH the dark-overlay popup that explains the override countdown
/// Pick the starting state with `initially_show_popup`. The (!) info badge
/// re-opens the popup at any time; the X inside dismisses it. Continue
/// proceeds to the override-password prompt in both states.
///
/// Callbacks:
///   on_continue    — Continue button → override password
///   on_back        — return to the previous screen (the WiFi list)
///   on_retry_wifi  — user wants to scan again (bounces to wifi_settings)
///   on_settings    — open the device Settings menu
std::shared_ptr<egt::Widget> create_wifi_unavailable_screen(
    std::function<void()> on_continue,
    std::function<void()> on_back,
    std::function<void()> on_retry_wifi,
    std::function<void()> on_settings,
    bool initially_show_popup = false);
