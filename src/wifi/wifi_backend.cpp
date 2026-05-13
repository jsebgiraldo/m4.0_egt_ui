#include "wifi_backend.h"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <array>
#include <algorithm>
#include <chrono>
#include <atomic>

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

    // Mock data when EGT_MOCK_WIFI=1 is set. The mock is DYNAMIC: each call
    // returns a slightly different set so the WiFi settings screen's
    // "refresh in place" code path is exercised:
    //  - Signal strength fluctuates ±15 around a base value
    //  - 1 of 12 APs is "transient" (visible only every other scan), so the
    //    UI sees APs entering/leaving the list naturally.
    //  - One "connected" AP rotates after a while to simulate roaming.
    // EGT_MOCK_WIFI=1 forces this path regardless of nmcli availability,
    // useful for iterating UI on a HW where NetworkManager is unstable.
    if (std::getenv("EGT_MOCK_WIFI")) {
        networks.clear();  // discard any real result; mock overrides
        // Persistent counter — increments on every scan_networks() call.
        static std::atomic<unsigned> mock_tick{0};
        unsigned tick = mock_tick.fetch_add(1);

        // Per-call deterministic "random" via tick — same tick always yields
        // the same list so two consecutive calls within the same poll window
        // produce identical results.
        auto wobble = [tick](int base, int slot) {
            // Pseudo-random ±15 jitter based on tick + slot
            int h = static_cast<int>((tick * 1103515245u + slot * 12345u) >> 8);
            int j = (h % 31) - 15;
            int v = base + j;
            if (v < 5)   v = 5;
            if (v > 100) v = 100;
            return v;
        };

        struct Base { const char* ssid; int signal; const char* security; };
        static const Base catalog[] = {
            {"SunTek-Office",     90, "WPA2"},
            {"BTWifi-Home",       78, "WPA2"},
            {"Starbucks-Free",    65, "Open"},
            {"ATT-Fiber-5G",      55, "WPA3"},
            {"Xfinity-Guest",     42, "WPA2"},
            {"TP-Link_8A3C",      35, "WPA2"},
            {"NETGEAR-Living",    28, "WPA2"},
            {"Hidden_Network_7",  20, "WPA2"},
            {"MAB-Manizales",     58, "WPA2"},
            {"OpenWrt-Lab",       72, "WPA3"},
            {"FAMILIA-Castano",   48, "WPA2"},
            {"Neighbour_5G",      38, "WPA2"},
        };

        const int total = sizeof(catalog) / sizeof(catalog[0]);
        // Pick a "transient" AP — visible only on even ticks
        int transient_slot = 9 + (tick / 6) % 3;  // rotates between 9..11
        // Pick the "connected" SSID — switches every 10 ticks
        int connected_slot = (tick / 10) % 3;     // first 3 SSIDs

        for (int i = 0; i < total; ++i) {
            // Skip the transient one on odd ticks
            if (i == transient_slot && (tick % 2) == 1) continue;
            WiFiNetwork n;
            n.ssid     = catalog[i].ssid;
            n.signal   = wobble(catalog[i].signal, i);
            n.security = catalog[i].security;
            n.connected = (i == connected_slot);
            networks.push_back(n);
        }

        if (tick == 0) {
            printf("[WIFI] Using DYNAMIC mock data (EGT_MOCK_WIFI=1)\n");
            fflush(stdout);
        }
        printf("[WIFI] mock tick=%u apset=%zu transient=%d connected=%d\n",
               tick, networks.size(), transient_slot, connected_slot);
        fflush(stdout);
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
    // Use device-state query instead of "dev wifi" which triggers a slow scan.
    // Format: DEVICE:TYPE:STATE:CONNECTION  e.g. wlan0:wifi:connected:MySSID
    std::string cmd = "nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device 2>/dev/null";
    std::istringstream stream(run_command(cmd));
    std::string line;
    while (std::getline(stream, line)) {
        // Match any wifi device in connected state: "*:wifi:connected:<ssid>"
        auto p1 = line.find(":wifi:connected:");
        if (p1 != std::string::npos) {
            std::string ssid = line.substr(p1 + std::string(":wifi:connected:").size());
            while (!ssid.empty() && (ssid.back() == '\n' || ssid.back() == '\r' || ssid.back() == ' '))
                ssid.pop_back();
            if (!ssid.empty()) return ssid;
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
