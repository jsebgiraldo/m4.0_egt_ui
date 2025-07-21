#pragma once

#include <string>
#include <vector>

namespace egt_wifi {

struct WiFiNetwork {
    std::string ssid;
    int signal;             // 0–100
    std::string security;
    bool connected;
};

class WiFiManager {
public:
    std::vector<WiFiNetwork> scan_networks();
    bool connect(const std::string& ssid, const std::string& password);
    bool disconnect();
    std::string get_current_ssid();
};

} // namespace egt_wifi
