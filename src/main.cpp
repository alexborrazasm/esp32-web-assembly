#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "console.h"
#include "wasm_runner.h"
#include "web.h"
#include "wifi_setup.h"

static AsyncWebServer server(80);

// No reserved words: everything typed in the web console goes to the module's stdin.
// Run / Stop / Reset are buttons backed by HTTP endpoints (see web.cpp).
static void onConsoleInput(const String &text)
{
    String line = text;
    line.trim();
    console_printf("> %s\n", line.c_str());
    wasm_runner_input(text);
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    if (!LittleFS.begin(true))
        Serial.println("[fs] failed to mount LittleFS");

    wifi_setup();
    web_begin(server);

    console_on_input(onConsoleInput);
    wasm_runner_begin();

    console_print("esp32-wasm ready\n");
    if (LittleFS.exists(MODULE_PATH))
    {
        if (wasm_runner_autorun())
            wasm_runner_run();
        else
            console_print("[wasm] module was stopped before the reboot, press Run to start it\n");
    }
}

void loop()
{
    delay(1000);
}
