#pragma once

#include <memory>
#include <functional>
#include <egt/widget.h>
#include "../wifi/wifi_backend.h"

std::shared_ptr<egt::Widget> create_wifi_network_details_screen(
    const egt_wifi::WiFiNetwork& network,
    std::function<void()> on_back,
    std::function<void(const std::string& ssid, const std::string& password)> on_connect
);