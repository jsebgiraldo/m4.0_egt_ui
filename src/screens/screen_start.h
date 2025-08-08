#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

std::shared_ptr<egt::Widget> create_start_screen(std::function<void()> on_next);