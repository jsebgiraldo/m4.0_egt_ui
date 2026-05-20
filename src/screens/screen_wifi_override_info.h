#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_OVERRIDE_INFO screen (Figma 134:1420 / State 2).
/// Orange banner "Wi-Fi/Network Connection remains unavailable, device will
/// continue to operate normally for N additional calendar day(s)", with a
/// Continue button (→ override password), an info (!) badge that opens an
/// explanatory popup, and Back / Retry WiFi / Setting at the bottom.
std::shared_ptr<egt::Widget> create_wifi_override_info_screen(
    std::function<void()> on_continue,
    std::function<void()> on_back,
    std::function<void()> on_retry_wifi,
    std::function<void()> on_settings);
