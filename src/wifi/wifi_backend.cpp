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

    // Mock data when nmcli is unavailable (e.g. simulator without NetworkManager)
    if (networks.empty() && std::getenv("EGT_MOCK_WIFI")) {
        printf("[WIFI] Using mock WiFi data (EGT_MOCK_WIFI set)\n");
        fflush(stdout);
        networks = {
            {"SunTek-Office",     92, "WPA2", true},
            {"BTWifi-Home",       78, "WPA2", false},
            {"Starbucks-Free",    65, "Open", false},
            {"ATT-Fiber-5G",      55, "WPA3", false},
            {"Xfinity-Guest",     42, "WPA2", false},
            {"TP-Link_8A3C",      35, "WPA2", false},
            {"NETGEAR-Living",    28, "WPA2", false},
            {"Hidden_Network_7",  20, "WPA2", false},
        };
    }

    return networks;
}

bool WiFiManager::connect(const std::string& ssid, const std::string& password) {
    // In mock mode, simulate successful connection
    if (std::getenv("EGT_MOCK_WIFI")) {
        printf("[WIFI] mock connect to '%s' -> OK\n", ssid.c_str());
        fflush(stdout);
        return true;
    }

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

    // Mock connected SSID when running in simulator
    // Use EGT_MOCK_WIFI=connected to simulate being already connected
    const char* mock = std::getenv("EGT_MOCK_WIFI");
    if (mock && std::string(mock) == "connected")
        return "SunTek-Office";

    return "";
}

bool WiFiManager::is_connected() {
    return !get_current_ssid().empty();
}

bool WiFiManager::is_available() {
    if (std::getenv("EGT_MOCK_WIFI"))
        return true;
    // nmcli exits with error and prints to stderr when NM not running;
    // stdout will be empty. Redirect stderr so run_command sees the message.
    std::string output = run_command("nmcli general status 2>&1");
    return output.find("NetworkManager is not running") == std::string::npos
        && !output.empty();
}

bool WiFiManager::has_saved_networks() {
    if (std::getenv("EGT_MOCK_WIFI"))
        return false;
    std::string cmd = "nmcli -t -f TYPE connection show 2>/dev/null";
    std::string output = run_command(cmd);
    // Each line is a connection type; wifi entries mean saved networks
    return output.find("802-11-wireless") != std::string::npos;
}

} // namespace egt_wifi
