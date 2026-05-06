#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// Settings menu screen with WiFi and Brightness options.
std::shared_ptr<egt::Widget> create_settings_screen(
    std::function<void()> on_back,
    std::function<void()> on_wifi_settings);
