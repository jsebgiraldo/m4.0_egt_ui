#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

// Solo mantener la función con Wi-Fi
std::shared_ptr<egt::Widget> create_start_screen_with_wifi(
    std::function<void()> on_next,
    std::function<void()> on_connection_complete,
    std::function<void()> on_connection_failed
);