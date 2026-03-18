#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>
#include <vector>

/// Technician profile data.
struct TechnicianProfile {
    std::string name;
    std::string password;
    // avatar_path can be added later when images are available
};

/// LOGIN screen (Figma Group 73:1664) — 2×3 card grid of technicians + Back.
/// Selecting a card opens the password keyboard.
std::shared_ptr<egt::Widget> create_login_screen_v2(
    const std::vector<TechnicianProfile>& technicians,
    std::function<void(const std::string& technician_name)> on_login_success,
    std::function<void()> on_back,
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen);
