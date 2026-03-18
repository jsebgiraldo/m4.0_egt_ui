#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

/// Patient data collected during intake.
struct PatientInfo {
    std::string gender;   // "Male" or "Female"
    int age = 0;
    std::string zip_code;
};

/// Patient info wizard (Figma Group 231).
/// Step 1: Gender → Step 2: Age Range → Step 3: ZIP Code.
/// on_complete is called with the collected data.
std::shared_ptr<egt::Widget> create_patient_info_screen(
    bool demo_mode,
    std::function<void(const PatientInfo& info)> on_complete,
    std::function<void()> on_back,
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen,
    std::function<void()> on_leave_demo = nullptr);
