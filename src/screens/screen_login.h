#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

// Función original (para compatibilidad)
std::shared_ptr<egt::Widget> create_login_screen(
    std::function<void()> on_success, 
    std::function<void()> on_cancel,
    std::function<void(const std::string&)> on_select_user
);

// Nueva función con navegación
std::shared_ptr<egt::Widget> create_login_screen_with_navigation(
    std::function<void()> on_success,
    std::function<void()> on_cancel, 
    std::function<void(const std::string&)> on_select_user,
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen
);