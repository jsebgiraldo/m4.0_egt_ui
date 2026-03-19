#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include <memory>
#include <string>
#include <functional>
#include <egt/widget.h>

#include "../wifi/wifi_backend.h"

/// Crea la pantalla de configuración Wi-Fi.
///
/// @param on_back Callback para el botón "Back".
/// @param on_scan_wifi Callback para iniciar escaneo de redes Wi-Fi.
/// @param on_connect Callback con los parámetros SSID y contraseña seleccionados.
/// @param on_item_selected Callback para cuando se selecciona un elemento de la lista.
/// @param on_show_screen Callback para mostrar pantallas (transiciones).
///
/// @return Widget principal de la pantalla.
std::shared_ptr<egt::Widget> create_wifi_settings_panel(
    std::function<void()> on_back,
    std::function<void()> on_scan_wifi,
    std::function<void(const std::string& ssid, const std::string& password)> on_connect,
    std::function<void(const egt_wifi::WiFiNetwork&)> on_item_selected,
    std::function<void(std::shared_ptr<egt::Widget>)> on_show_screen = nullptr,
    int scroll_offset = 0,
    std::shared_ptr<std::vector<egt_wifi::WiFiNetwork>> cached_networks = nullptr
);

#endif // SCREEN_SETTINGS_H
