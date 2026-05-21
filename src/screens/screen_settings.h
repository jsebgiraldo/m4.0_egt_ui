#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// Settings menu screen with WiFi and Brightness options.
/// `on_login` (optional) shows a "Technician Login" button that returns to
/// the Login screen; pass nullptr to hide it.
std::shared_ptr<egt::Widget> create_settings_screen(
    std::function<void()> on_back,
    std::function<void()> on_wifi_settings,
    std::function<void()> on_login = nullptr);
