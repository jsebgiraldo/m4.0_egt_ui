#include "override_config.h"

#include <fstream>
#include <string>

namespace ui {

// Persisted in a plain text file so a factory tech can change it in the
// field, and so the value survives reboots. Falls back to the default if
// the file is missing or unreadable.
static const char* kOverrideDaysPath = "/etc/m4-egt/override_days";
static constexpr int kDefaultDays = 7;
static constexpr int kMinDays = 1;
static constexpr int kMaxDays = 30;

static int clamp_days(int d) {
    if (d < kMinDays) return kMinDays;
    if (d > kMaxDays) return kMaxDays;
    return d;
}

int get_override_days() {
    std::ifstream f(kOverrideDaysPath);
    if (f.is_open()) {
        int v = -1;
        f >> v;
        if (v >= kMinDays) return clamp_days(v);
    }
    return kDefaultDays;
}

bool set_override_days(int days) {
    days = clamp_days(days);
    std::ofstream f(kOverrideDaysPath);
    if (!f.is_open()) return false;
    f << days;
    return f.good();
}

} // namespace ui
