#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

std::shared_ptr<egt::Widget> create_password_prompt_screen(
    const std::string& title,
    const std::string& message,
    const std::string& join_label,
    const std::string& cancel_label,
    std::function<void(const std::string& password)> on_join,
    std::function<void()> on_cancel);