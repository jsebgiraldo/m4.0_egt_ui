#include "screen_wifi_settings.h"

std::shared_ptr<egt::Box> create_settings_screen(std::function<void()> on_back)
{
    auto box = std::make_shared<egt::Box>(egt::Orientation::vertical);
    box->add(std::make_shared<egt::Label>("Settings"));

    auto btn_back = std::make_shared<egt::Button>("Back");
    btn_back->on_click([=](egt::Event&) { on_back(); });
    box->add(btn_back);

    return box;
}