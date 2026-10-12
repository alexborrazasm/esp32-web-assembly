#pragma once

#include <ESPAsyncWebServer.h>

// Routes: / (LittleFS), POST /upload (.wasm), GET /api/status, POST /api/{run,stop,clear,reset},
// WS /console
void web_begin(AsyncWebServer &server);

// Path of the last uploaded module in LittleFS
constexpr const char *MODULE_PATH = "/module.wasm";
