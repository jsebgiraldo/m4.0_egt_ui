#include <egt/ui>
#include "screen_info.h"

#include <dbus/dbus.h>
#include <iostream>

using namespace egt;
using namespace std;

bool set_wifi_enabled(bool enable)
{
    DBusError err;
    DBusConnection* conn;
    dbus_error_init(&err);

    // Conectarse al bus del sistema
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        std::cerr << "DBus error (connect): " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    // Crear el mensaje
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.NetworkManager",           // destination
        "/org/freedesktop/NetworkManager",          // path
        "org.freedesktop.NetworkManager",           // interface
        "SetWirelessEnabled"                        // method
    );
    if (!msg) {
        std::cerr << "Failed to create message" << std::endl;
        return false;
    }

    // Agregar argumento booleano
    dbus_bool_t dbus_enable = enable ? TRUE : FALSE;
    if (!dbus_message_append_args(msg, DBUS_TYPE_BOOLEAN, &dbus_enable, DBUS_TYPE_INVALID)) {
        std::cerr << "Failed to append arguments" << std::endl;
        dbus_message_unref(msg);
        return false;
    }

    // Enviar el mensaje
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        std::cerr << "DBus error (call): " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    if (reply)
        dbus_message_unref(reply);

    return true;
}

shared_ptr<Widget> create_info_screen(function<void()> on_back)
{
    auto container = make_shared<Frame>(Rect(0, 0, 800, 480));
    auto label = make_shared<Label>("Info Screen", Rect(100, 40, 200, 40));
    label->align(AlignFlag::center_horizontal);
    container->add(label);

    auto btn_toggle = std::make_shared<Button>("Toggle Wi-Fi", Rect(10, 80, 200, 40));
    btn_toggle->align(AlignFlag::center_horizontal);
    btn_toggle->on_click([](Event&) {
    static bool wifi_on = false;
    wifi_on = !wifi_on;
    bool ok = set_wifi_enabled(wifi_on);
    std::cout << "Wi-Fi " << (wifi_on ? "enabled" : "disabled") << " -> " << (ok ? "OK" : "ERROR") << std::endl;
});
    container->add(btn_toggle);

    auto btn = make_shared<Button>("Back", Rect(800/2 - 50, 400, 100, 40));
    btn->align(AlignFlag::center_horizontal);
    btn->on_click([=](Event&) { on_back(); });
    container->add(btn);



    return container;
}