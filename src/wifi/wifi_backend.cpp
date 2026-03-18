#include "wifi_backend.h"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <array>

namespace egt_wifi {

static std::string run_command(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    return result;
}

std::vector<WiFiNetwork> WiFiManager::scan_networks() {
    std::vector<WiFiNetwork> networks;
    std::string cmd = "nmcli -t -f SSID,SIGNAL,SECURITY,IN-USE dev wifi";
    std::istringstream stream(run_command(cmd));
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        WiFiNetwork net;
        std::stringstream linestream(line);
        std::string token;
        std::getline(linestream, net.ssid, ':');
        std::getline(linestream, token, ':'); // SIGNAL
        net.signal = std::stoi(token);
        std::getline(linestream, net.security, ':');
        std::getline(linestream, token, ':'); // IN-USE
        net.connected = (token == "*");
        networks.push_back(net);
    }

    return networks;
}

bool WiFiManager::connect(const std::string& ssid, const std::string& password) {
    std::string cmd = "nmcli dev wifi connect \"" + ssid + "\" password \"" + password + "\"";
    std::string output = run_command(cmd);
    return output.find("successfully activated") != std::string::npos;
}

bool WiFiManager::disconnect() {
    std::string cmd = "nmcli networking off && sleep 1 && nmcli networking on";
    return !run_command(cmd).empty();
}

std::string WiFiManager::get_current_ssid() {
    std::string cmd = "nmcli -t -f ACTIVE,SSID dev wifi | grep '^yes' | cut -d: -f2";
    return run_command(cmd);
}

} // namespace egt_wifi
