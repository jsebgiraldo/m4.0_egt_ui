#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>
#include <vector>

// One action button on the treatment error modal (Figma 143:870/871: white
// card, gray bold label, soft drop shadow). `sublabel` adds a smaller second
// line (e.g. the "Treatment" under "Begin"); leave it empty for a single line.
struct ErrorAction {
    std::string label;
    std::string sublabel;
    std::function<void()> on_click;
};

// Treatment error / warning modal (Figma frames 143:870..143:874): a centred
// gray card with a rounded-top header carrying a white-disc icon and a white
// title, a gray body message, and 0-3 action buttons along the bottom.
// Errors use a blue header with Pause / Resume / End (143:870/871); warnings
// use an orange header and no buttons (143:873/874).
//   icon_asset   — path to the white-disc SVG glyph under assets/figma/icons
//   message      — body text; embed '\n' to control line breaks
//   actions      — bottom buttons (empty for a button-less warning)
//   header_color — header banner color (default blue #305FC4; orange for warnings)
std::shared_ptr<egt::Widget> create_error_screen(
    const std::string& icon_asset,
    const std::string& title,
    const std::string& message,
    const std::vector<ErrorAction>& actions,
    const egt::Color& header_color = egt::Color(0x30, 0x5F, 0xC4));
