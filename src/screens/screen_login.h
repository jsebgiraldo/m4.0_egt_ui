#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

std::shared_ptr<egt::Widget> create_login_screen(
    std::function<void()> on_success, 
    std::function<void()> on_cancel
);