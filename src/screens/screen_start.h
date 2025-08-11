#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

// Versión simple (actual)
std::shared_ptr<egt::Widget> create_start_screen(std::function<void()> on_next);

// Versión avanzada con callbacks de conexión
std::shared_ptr<egt::Widget> create_start_screen_with_wifi(
    std::function<void()> on_next,
    std::function<void()> on_connection_complete = nullptr,
    std::function<void()> on_connection_failed = nullptr
);