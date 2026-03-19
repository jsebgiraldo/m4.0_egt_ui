#include "wifi_backend.h"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <array>
#include <algorithm>

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

// Sanitize a string for safe use in shell single-quotes (replace ' with '\'' )
static std::string shell_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    return out;
}

std::vector<WiFiNetwork> WiFiManager::scan_networks() {
    std::vector<WiFiNetwork> networks;
    // Trigger a rescan first (ignore errors)
    run_command("nmcli dev wifi rescan 2>/dev/null");
    std::string cmd = "nmcli -t -f SSID,SIGNAL,SECURITY,IN-USE dev wifi";
    std::istringstream stream(run_command(cmd));
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        WiFiNetwork net;
        std::stringstream linestream(line);
        std::string token;
        std::getline(linestream, net.ssid, ':');
        if (net.ssid.empty()) continue;
        std::getline(linestream, token, ':'); // SIGNAL
        try { net.signal = std::stoi(token); } catch (...) { net.signal = 0; }
        std::getline(linestream, net.security, ':');
        std::getline(linestream, token, ':'); // IN-USE
        net.connected = (token.find('*') != std::string::npos);
        networks.push_back(net);
    }

    return networks;
}

bool WiFiManager::connect(const std::string& ssid, const std::string& password) {
    std::string cmd = "nmcli dev wifi connect '" +
        shell_escape(ssid) + "' password '" +
        shell_escape(password) + "'";
    printf("[WIFI] connect: nmcli dev wifi connect '%s'\n", ssid.c_str());
    std::string output = run_command(cmd);
    printf("[WIFI] connect result: %s\n", output.c_str());
    return output.find("successfully activated") != std::string::npos;
}

bool WiFiManager::disconnect() {
    std::string cmd = "nmcli dev disconnect wlan0";
    return !run_command(cmd).empty();
}

std::string WiFiManager::get_current_ssid() {
    std::string cmd = "nmcli -t -f ACTIVE,SSID dev wifi";
    std::istringstream stream(run_command(cmd));
    std::string line;
    while (std::getline(stream, line)) {
        if (line.substr(0, 4) == "yes:") {
            std::string ssid = line.substr(4);
            // Trim whitespace
            while (!ssid.empty() && (ssid.back() == '\n' || ssid.back() == '\r' || ssid.back() == ' '))
                ssid.pop_back();
            return ssid;
        }
    }
    return "";
}

bool WiFiManager::is_connected() {
    return !get_current_ssid().empty();
}

} // namespace egt_wifi
