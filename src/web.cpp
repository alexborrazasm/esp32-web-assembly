#include "web.h"

#include <LittleFS.h>
#include <WiFi.h>

#include "console.h"
#include "wasm_runner.h"

static constexpr const char *TMP_PATH = "/module.tmp";
static constexpr size_t MAX_MODULE_SIZE = 4 * 1024 * 1024;

// State of the upload in progress (only one at a time)
static File uploadFile;
static bool uploadOk = false;
static size_t uploadSize = 0;
static String uploadError;

static void abortUpload(const char *msg)
{
    uploadError = msg;
    if (uploadFile)
        uploadFile.close();
    LittleFS.remove(TMP_PATH);
}

static void onUploadData(AsyncWebServerRequest *, const String &, size_t index, uint8_t *data, size_t len, bool final)
{
    static const uint8_t magic[4] = {0x00, 'a', 's', 'm'};

    if (index == 0)
    {
        uploadOk = false;
        uploadSize = 0;
        uploadError = "";
        if (len < 4 || memcmp(data, magic, 4) != 0)
        {
            uploadError = "not a wasm module (missing \\0asm header)";
            return;
        }
        uploadFile = LittleFS.open(TMP_PATH, "w");
        if (!uploadFile)
        {
            uploadError = "could not create temporary file";
            return;
        }
    }

    if (!uploadFile)
        return; // an error already happened, ignore the rest

    if (index + len > MAX_MODULE_SIZE)
        return abortUpload("module too large");
    if (uploadFile.write(data, len) != len)
        return abortUpload("LittleFS full");

    if (final)
    {
        uploadSize = index + len;
        uploadFile.close();
        // The previous module is only lost if the new one arrived complete
        LittleFS.remove(MODULE_PATH);
        if (LittleFS.rename(TMP_PATH, MODULE_PATH))
            uploadOk = true;
        else
            uploadError = "could not save module";
    }
}

static void onUploadDone(AsyncWebServerRequest *req)
{
    if (uploadOk)
    {
        console_printf("[upload] %u bytes saved\n", (unsigned)uploadSize);
        wasm_runner_run(); // hot reload
        req->send(200, "application/json", "{\"ok\":true,\"size\":" + String(uploadSize) + "}");
    }
    else
    {
        console_printf("[upload] rejected: %s\n", uploadError.c_str());
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"" + uploadError + "\"}");
    }
}

static void restartSoon(void *)
{
    vTaskDelay(pdMS_TO_TICKS(300)); // let the HTTP response go out first
    ESP.restart();
}

static void onReset(AsyncWebServerRequest *req)
{
    console_print("[sys] resetting...\n");
    req->send(200, "application/json", "{\"ok\":true}");
    xTaskCreatePinnedToCore(restartSoon, "restart", 2048, nullptr, 1, nullptr, 0); // core 1 may be saturated by the module
}

static void onClear(AsyncWebServerRequest *req)
{
    console_clear();
    req->send(200, "application/json", "{\"ok\":true}");
}

static void onStop(AsyncWebServerRequest *req)
{
    wasm_runner_stop();
    req->send(200, "application/json", "{\"ok\":true}");
}

static void onRun(AsyncWebServerRequest *req)
{
    wasm_runner_run();
    req->send(200, "application/json", "{\"ok\":true}");
}

static void onStatus(AsyncWebServerRequest *req)
{
    size_t moduleSize = 0;
    if (File f = LittleFS.open(MODULE_PATH, "r"))
    {
        moduleSize = f.size();
        f.close();
    }

    bool sta = WiFi.getMode() & WIFI_STA && WiFi.isConnected();
    String json = "{";
    json += "\"mode\":\"" + String(sta ? "sta" : "ap") + "\",";
    json += "\"ssid\":\"" + (sta ? WiFi.SSID() : String("esp32-wasm")) + "\",";
    json += "\"ip\":\"" + (sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"psram\":" + String(ESP.getFreePsram()) + ",";
    json += "\"module\":" + String(moduleSize) + ",";
    json += "\"running\":" + String(wasm_runner_is_running() ? "true" : "false") + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    req->send(200, "application/json", json);
}

void web_begin(AsyncWebServer &server)
{
    console_begin(server);

    server.on("/api/status", HTTP_GET, onStatus);
    server.on("/upload", HTTP_POST, onUploadDone, onUploadData);
    server.on("/api/reset", HTTP_POST, onReset);
    server.on("/api/stop", HTTP_POST, onStop);
    server.on("/api/clear", HTTP_POST, onClear);
    server.on("/api/run", HTTP_POST, onRun);

    // Automatically serves precompressed .gz files if present (xterm.js.gz, etc.)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache");

    server.onNotFound([](AsyncWebServerRequest *req)
                      { req->send(404, "text/plain", "not found"); });

    server.begin();
}
