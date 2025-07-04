#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

std::shared_ptr<egt::Widget> create_menu_screen(
    std::function<void()> on_monitor,
    std::function<void()> on_settings,
    std::function<void()> on_info,
    std::function<void()> on_exit);