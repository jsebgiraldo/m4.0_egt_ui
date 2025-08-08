#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

std::shared_ptr<egt::Widget> create_training_screen(
    int duration_seconds,
    std::function<void()> on_complete,
    std::function<void()> on_pause,
    std::function<void()> on_cancel
);