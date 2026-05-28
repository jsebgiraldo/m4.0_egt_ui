#pragma once
#include <egt/ui>
#include <memory>
#include <functional>

/// WIFI_NOT_FOUND — intro screen shown when the device fails to find any
/// available networks (Figma node 52:2774 area, M4-19 "Wi-Fi Network not
/// found"). Hero "Operate without WiFi" button in the middle + small
/// [Retry WiFi] / [Setting] row at the bottom. Tapping the hero button
/// enters the override-info flow (the popup with the countdown / password).
std::shared_ptr<egt::Widget> create_wifi_not_found_screen(
    std::function<void()> on_operate_without_wifi,
    std::function<void()> on_retry_wifi,
    std::function<void()> on_settings);
