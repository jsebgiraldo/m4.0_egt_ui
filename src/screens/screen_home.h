#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// HOME screen (Figma Group 197).
/// Layout: Logo centered, large "Begin Treatment" button,
/// small "Demo Mode" (bottom-left) and "Setting" (bottom-right).
std::shared_ptr<egt::Widget> create_home_screen(
    std::function<void()> on_begin_treatment,
    std::function<void()> on_demo_mode,
    std::function<void()> on_settings);
