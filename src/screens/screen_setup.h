#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// SETUP landing screen (Figma 151:861).
/// Centered Lice Clinics logo + a "Settings" affordance (gear in a gray
/// circle + label) at the bottom-left. Reached when the user backs out of
/// the Wi-Fi list during the boot flow (so Back no longer skips straight to
/// Home / past Technician Login). Tapping Settings opens the device Settings.
std::shared_ptr<egt::Widget> create_setup_screen(
    std::function<void()> on_settings);
