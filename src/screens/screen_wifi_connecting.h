#pragma once
#include <egt/ui>
#include <memory>
#include <functional>
#include <string>

/// WIFI_CONNECTING screen — transient screen shown while a connection attempt
/// is in flight. Runs WiFiManager::connect() on a background thread (the call
/// can block 5-10s associating + DHCP) and polls the result on the UI thread,
/// so the UI never freezes. Navigates via on_success / on_failure.
std::shared_ptr<egt::Widget> create_wifi_connecting_screen(
    const std::string& ssid,
    const std::string& password,
    std::function<void()> on_success,
    std::function<void()> on_failure);
