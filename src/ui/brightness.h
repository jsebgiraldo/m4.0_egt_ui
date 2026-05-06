#pragma once

namespace ui {

/// Read current backlight brightness (0-100). Returns -1 if unavailable.
int get_brightness();

/// Set backlight brightness (0-100). Returns true on success.
bool set_brightness(int percent);

} // namespace ui
