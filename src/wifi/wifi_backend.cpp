#include "wifi_backend.h"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <array>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <thread>

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

static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

// ── wpa_cli fallback ───────────────────────────────────────────────────────
// Used when NetworkManager doesn't manage wlan0 (e.g. rc8 image where wlan0 is
// marked unmanaged to mitigate the SDHCI bug). wpa_supplicant must be running
// for these to work — typically launched manually via:
//   wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
static std::string wpa_iface_cache;
static std::string wpa_iface() {
    if (!wpa_iface_cache.empty()) return wpa_iface_cache;
    std::string out = trim(run_command(
        "ls /var/run/wpa_supplicant/ 2>/dev/null | head -1"));
    wpa_iface_cache = out;
    return out;
}

static bool wpa_available() {
    std::string iface = wpa_iface();
    if (iface.empty()) return false;
    std::string ping = run_command(
        "wpa_cli -i " + iface + " ping 2>/dev/null");
    return ping.find("PONG") != std::string::npos;
}

static std::string wpa_cli(const std::string& cmd) {
    std::string iface = wpa_iface();
    if (iface.empty()) return "";
    return run_command("wpa_cli -i " + iface + " " + cmd + " 2>/dev/null");
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

    // Fallback: NM didn't return any networks (wlan0 likely unmanaged).
    // Use wpa_cli directly if wpa_supplicant is running.
    if (networks.empty() && wpa_available() && !std::getenv("EGT_MOCK_WIFI")) {
        printf("[WIFI] nmcli returned empty, falling back to wpa_cli scan\n");
        fflush(stdout);
        wpa_cli("scan");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::string results = wpa_cli("scan_results");
        // First line is header; rest are tab-separated:
        //   bssid  freq  signal_dbm  flags  ssid
        std::istringstream rs(results);
        std::string l;
        std::getline(rs, l); // header
        while (std::getline(rs, l)) {
            if (l.empty()) continue;
            std::vector<std::string> parts;
            std::stringstream ls(l);
            std::string p;
            while (std::getline(ls, p, '\t')) parts.push_back(p);
            if (parts.size() < 5) continue;
            WiFiNetwork net;
            net.ssid = parts[4];
            if (net.ssid.empty()) continue;
            int dbm = -90;
            try { dbm = std::stoi(parts[2]); } catch (...) {}
            // Convert dBm (-90..-30) to 0..100 signal strength
            int sig = 2 * (dbm + 100);
            if (sig < 0) sig = 0;
            if (sig > 100) sig = 100;
            net.signal = sig;
            const std::string& flags = parts[3];
            if      (flags.find("WPA3") != std::string::npos) net.security = "WPA3";
            else if (flags.find("WPA2") != std::string::npos) net.security = "WPA2";
            else if (flags.find("WPA")  != std::string::npos) net.security = "WPA";
            else if (flags.find("WEP")  != std::string::npos) net.security = "WEP";
            else net.security = "Open";
            net.connected = false;
            networks.push_back(net);
        }
        // Mark connected SSID from wpa_cli status
        std::string st = wpa_cli("status");
        std::string cur_ssid;
        std::istringstream sts(st);
        std::string stline;
        bool completed = false;
        while (std::getline(sts, stline)) {
            if (stline.rfind("wpa_state=COMPLETED", 0) == 0) completed = true;
            else if (stline.rfind("ssid=", 0) == 0) cur_ssid = stline.substr(5);
        }
        if (completed && !cur_ssid.empty()) {
            for (auto& n : networks)
                if (n.ssid == cur_ssid) { n.connected = true; break; }
        }
        printf("[WIFI] wpa_cli scan: %zu APs (current='%s')\n",
               networks.size(), cur_ssid.c_str());
        fflush(stdout);
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

    // Try NM first
    std::string cmd = "nmcli dev wifi connect '" +
        shell_escape(ssid) + "' password '" +
        shell_escape(password) + "'";
    printf("[WIFI] connect: nmcli dev wifi connect '%s'\n", ssid.c_str());
    std::string output = run_command(cmd);
    printf("[WIFI] connect result: %s\n", output.c_str());
    if (output.find("successfully activated") != std::string::npos)
        return true;

    // Fallback: wpa_cli (NM doesn't manage wlan0)
    if (!wpa_available()) return false;
    printf("[WIFI] falling back to wpa_cli connect\n");
    fflush(stdout);
    std::string id = trim(wpa_cli("add_network"));
    if (id.empty() || id.find("FAIL") != std::string::npos) return false;
    // Quoted values per wpa_cli convention; shell-escape for the outer ''
    wpa_cli("set_network " + id + " ssid '\"" + shell_escape(ssid) + "\"'");
    if (password.empty()) {
        wpa_cli("set_network " + id + " key_mgmt NONE");
    } else {
        wpa_cli("set_network " + id + " psk '\"" + shell_escape(password) + "\"'");
    }
    wpa_cli("enable_network " + id);
    wpa_cli("select_network " + id);
    wpa_cli("save_config");
    // Poll for COMPLETED state
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::string st = wpa_cli("status");
        if (st.find("wpa_state=COMPLETED") != std::string::npos) {
            // Trigger DHCP for IP
            run_command("udhcpc -i " + wpa_iface() + " -n -q -t 5 2>&1");
            printf("[WIFI] wpa_cli connect to '%s' -> OK\n", ssid.c_str());
            fflush(stdout);
            return true;
        }
    }
    printf("[WIFI] wpa_cli connect timed out\n");
    fflush(stdout);
    return false;
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

    // Fallback: NM doesn't report wifi → query wpa_cli directly
    if (wpa_available() && !std::getenv("EGT_MOCK_WIFI")) {
        std::string st = wpa_cli("status");
        std::istringstream sts(st);
        std::string stline, cur_ssid;
        bool completed = false;
        while (std::getline(sts, stline)) {
            if (stline.rfind("wpa_state=COMPLETED", 0) == 0) completed = true;
            else if (stline.rfind("ssid=", 0) == 0) cur_ssid = trim(stline.substr(5));
        }
        if (completed && !cur_ssid.empty()) return cur_ssid;
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
    // NM is the preferred control plane. If it's running, assume WiFi is OK
    // even if it doesn't currently manage wlan0 — we'll fall back to wpa_cli
    // for scan/connect/status. is_available() just gates the WIFI_INIT flow.
    std::string output = run_command("nmcli general status 2>&1");
    bool nm_ok = output.find("NetworkManager is not running") == std::string::npos
                 && !output.empty();
    if (nm_ok) return true;
    // NM down → check wpa_supplicant directly
    return wpa_available();
}

bool WiFiManager::has_saved_networks() {
    if (std::getenv("EGT_MOCK_WIFI"))
        return false;
    std::string cmd = "nmcli -t -f TYPE connection show 2>/dev/null";
    std::string output = run_command(cmd);
    if (output.find("802-11-wireless") != std::string::npos) return true;
    // Fallback: wpa_cli list_networks (skip header line)
    if (wpa_available()) {
        std::string nets = wpa_cli("list_networks");
        std::istringstream ns(nets);
        std::string l;
        std::getline(ns, l); // header "network id / ssid / bssid / flags"
        while (std::getline(ns, l)) {
            if (!trim(l).empty()) return true;
        }
    }
    return false;
}

} // namespace egt_wifi
