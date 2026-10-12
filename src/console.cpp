#include "console.h"

#include <freertos/stream_buffer.h>
#include <stdarg.h>

static AsyncWebSocket ws("/console");
static SemaphoreHandle_t lock;
static StreamBufferHandle_t outBuf; // producers (any core) -> pump task (core 0)
static ConsoleInputHandler inputHandler;

static constexpr size_t RING_SIZE = 8192;
static char ring[RING_SIZE];
static size_t ringHead = 0; // next write position
static size_t ringLen = 0;

// xterm.js needs \r\n; converted before storing in the history.
static String toTerminal(const char *data, size_t len)
{
    String out;
    out.reserve(len + 8);
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] == '\n' && (i == 0 || data[i - 1] != '\r'))
            out += '\r';
        out += data[i];
    }
    return out;
}

static void ringAppend(const String &s)
{
    for (size_t i = 0; i < s.length(); i++)
    {
        ring[ringHead] = s[i];
        ringHead = (ringHead + 1) % RING_SIZE;
        if (ringLen < RING_SIZE)
            ringLen++;
    }
}

static String ringSnapshot()
{
    String out;
    out.reserve(ringLen);
    size_t start = (ringHead + RING_SIZE - ringLen) % RING_SIZE;
    for (size_t i = 0; i < ringLen; i++)
        out += ring[(start + i) % RING_SIZE];
    return out;
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        xSemaphoreTake(lock, portMAX_DELAY);
        String history = ringSnapshot();
        xSemaphoreGive(lock);
        if (history.length())
            client->text(history);
    }
    else if (type == WS_EVT_DATA)
    {
        auto *info = (AwsFrameInfo *)arg;
        // Only complete text messages in a single frame (terminal keystrokes)
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT && inputHandler)
            inputHandler(String((const char *)data, len));
    }
}

// Drains outBuf on core 0: serial, history and WebSocket. Producers (e.g. the WASM
// runner on core 1) never wait on the network.
static void pumpTask(void *)
{
    char chunk[256];
    for (;;)
    {
        size_t n = xStreamBufferReceive(outBuf, chunk, sizeof(chunk), portMAX_DELAY);
        if (!n)
            continue;

        Serial.write((const uint8_t *)chunk, n);

        String term = toTerminal(chunk, n);
        xSemaphoreTake(lock, portMAX_DELAY);
        ringAppend(term);
        xSemaphoreGive(lock);

        ws.cleanupClients();
        if (ws.count())
            ws.textAll(term);
    }
}

void console_begin(AsyncWebServer &server)
{
    lock = xSemaphoreCreateMutex();
    outBuf = xStreamBufferCreate(8192, 1);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    xTaskCreatePinnedToCore(pumpTask, "console", 4096, nullptr, 1, nullptr, 0);
}

void console_write(const char *data, size_t len)
{
    if (!outBuf)
    {
        Serial.write((const uint8_t *)data, len); // before console_begin()
        return;
    }
    // Short wait for room; if the buffer stays full the output is dropped
    xStreamBufferSend(outBuf, data, len, pdMS_TO_TICKS(10));
}

void console_clear()
{
    xSemaphoreTake(lock, portMAX_DELAY);
    ringHead = 0;
    ringLen = 0;
    xSemaphoreGive(lock);

    ws.textAll("\x1b"
               "c"); // RIS: full terminal reset, scrollback included
}

void console_print(const char *text)
{
    console_write(text, strlen(text));
}

void console_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0)
        return;
    console_write(buf, min((size_t)n, sizeof(buf) - 1));
}

void console_on_input(ConsoleInputHandler handler)
{
    inputHandler = handler;
}
