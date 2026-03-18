#pragma once

#include <egt/ui>
#include <functional>
#include <memory>

std::shared_ptr<egt::Widget> create_demo_info_screen(
    std::function<void()> on_continue,
    std::function<void()> on_back);
