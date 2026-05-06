#include "brightness.h"
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>

namespace ui {

// SAMA5D27 backlight sysfs path — adjust if the path differs on your BSP
static const char* BACKLIGHT_PATH = "/sys/class/backlight/backlight/brightness";
static const char* BACKLIGHT_MAX_PATH = "/sys/class/backlight/backlight/max_brightness";
static constexpr int MIN_VISIBLE_HW_PERCENT = 21;

static int clamp_percent(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

// Map UI percentage [0..100] to hardware percentage with a safe visible floor.
// 0% in UI is intentionally not absolute black to keep the screen readable.
static int ui_percent_to_hw_percent(int ui_percent) {
    ui_percent = clamp_percent(ui_percent);
    const int span = 100 - MIN_VISIBLE_HW_PERCENT;
    return MIN_VISIBLE_HW_PERCENT + (ui_percent * span) / 100;
}

// Inverse mapping for displaying the value in UI.
static int hw_percent_to_ui_percent(int hw_percent) {
    hw_percent = clamp_percent(hw_percent);
    if (hw_percent <= MIN_VISIBLE_HW_PERCENT)
        return 0;

    const int span = 100 - MIN_VISIBLE_HW_PERCENT;
    return ((hw_percent - MIN_VISIBLE_HW_PERCENT) * 100) / span;
}

static int read_int_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return -1;
    int val = -1;
    f >> val;
    return val;
}

int get_brightness() {
    // Mock mode for simulator
    static int mock_brightness = 80;
    if (std::getenv("EGT_MOCK_WIFI")) return mock_brightness;

    int max_val = read_int_file(BACKLIGHT_MAX_PATH);
    int cur_val = read_int_file(BACKLIGHT_PATH);
    if (max_val <= 0 || cur_val < 0) return -1;
    int hw_percent = (cur_val * 100) / max_val;
    return hw_percent_to_ui_percent(hw_percent);
}

bool set_brightness(int percent) {
    percent = clamp_percent(percent);

    // Mock mode for simulator
    if (std::getenv("EGT_MOCK_WIFI")) {
        printf("[BRIGHTNESS] mock set to %d%%\n", percent);
        fflush(stdout);
        return true;
    }

    int max_val = read_int_file(BACKLIGHT_MAX_PATH);
    if (max_val <= 0) return false;

    int hw_percent = ui_percent_to_hw_percent(percent);
    int hw_val = (hw_percent * max_val) / 100;
    std::ofstream f(BACKLIGHT_PATH);
    if (!f.is_open()) return false;
    f << hw_val;
    return f.good();
}

} // namespace ui
