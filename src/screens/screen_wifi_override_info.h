#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_OVERRIDE_INFO screen (Figma 134:1420).
/// "Wi-Fi/Network Connection remains unavailable" informational screen
/// before entering override password.
std::shared_ptr<egt::Widget> create_wifi_override_info_screen(
    std::function<void()> on_continue,
    std::function<void()> on_back);
