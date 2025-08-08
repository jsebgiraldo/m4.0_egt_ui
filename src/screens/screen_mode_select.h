#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

std::shared_ptr<egt::Widget> create_mode_select_screen(
    std::function<void()> on_training,
    std::function<void()> on_treatment, 
    std::function<void()> on_back
);