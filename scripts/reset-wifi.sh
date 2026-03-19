#!/bin/sh
# Reset WiFi credentials and restart EGT app so it re-prompts for WiFi setup.
# Run on target: sh /usr/bin/reset-wifi.sh

echo "=== Deleting stored WiFi connections ==="
nmcli -t -f NAME,TYPE connection show | while IFS=: read -r name type; do
    if [ "$type" = "802-11-wireless" ]; then
        echo "  Removing: $name"
        nmcli connection delete "$name"
    fi
done

echo "=== Restarting EGT app ==="
systemctl restart egtapp

echo "Done. WiFi credentials cleared — app will prompt for new credentials."
