#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_UNAVAILABLE screen (Figma 134:1418).
/// "Wi-Fi Network is Temporarily Unavailable" with Continue button.
std::shared_ptr<egt::Widget> create_wifi_unavailable_screen(
    std::function<void()> on_continue);
