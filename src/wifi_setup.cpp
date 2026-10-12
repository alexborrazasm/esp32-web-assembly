#include "wifi_setup.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>

static constexpr const char *HOSTNAME = "esp32-wasm";
static constexpr const char *AP_NAME = "esp32-wasm";
static constexpr uint16_t PORTAL_TIMEOUT_S = 120;

// Dark theme for the config portal, same palette as the main web UI
static const char PORTAL_DARK_CSS[] = R"(<style>
:root{color-scheme:dark}
body{background:#14161a;color:#d8dce3}
a{color:#60a5fa}
input,select{background:#1d2026;color:#d8dce3;border:1px solid #2d323b;border-radius:6px}
input:focus,select:focus{outline:1px solid #60a5fa}
button{background:#2563eb;color:#fff;border-radius:6px}
button.D{background:#b91c1c}
.q{color:#8b93a1}
.msg{background:#1d2026;border-color:#60a5fa;color:#d8dce3}
.msg.S{border-color:#4ade80}
.msg.D{border-color:#f87171}
</style>)";

void wifi_setup()
{
    // WiFiManager starts its own server on port 80: it lives only inside this
    // function so the port is released before the main server starts.
    {
        WiFiManager wm;
        wm.setHostname(HOSTNAME);
        wm.setCustomHeadElement(PORTAL_DARK_CSS);
        wm.setConnectTimeout(15);
        wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

        if (!wm.autoConnect(AP_NAME))
        {
            // No network and nobody configured the portal: plain open AP
            Serial.println("[wifi] no connection, AP mode");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(AP_NAME);
        }
    }

    if (WiFi.getMode() & WIFI_STA && WiFi.isConnected())
        Serial.printf("[wifi] connected to %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    else
        Serial.printf("[wifi] AP '%s', IP %s\n", AP_NAME, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin(HOSTNAME))
        Serial.printf("[wifi] mDNS: http://%s.local\n", HOSTNAME);
}
