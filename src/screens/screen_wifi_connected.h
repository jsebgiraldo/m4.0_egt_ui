#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_CONNECTED screen — shown when wifi_init successfully reaches a connected
/// state. Figma node 2065:980 child (Connected card): logo + green check circle
/// + "Wi-Fi Connected" title + subtitle + Continue button (manual gate before
/// advancing to Login).
std::shared_ptr<egt::Widget> create_wifi_connected_screen(
    std::function<void()> on_continue);
