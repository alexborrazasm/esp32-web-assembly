#include "wasm_runner.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <wasm3.h>

#include "console.h"
#include "host_api.h"
#include "web.h"

static constexpr uint32_t RUNNER_STACK_BYTES = 32 * 1024; // C stack of the runner task
static constexpr uint32_t WASM_STACK_BYTES = 64 * 1024;   // wasm3 value stack
static constexpr int RUNNER_CORE = 1;

static QueueHandle_t runQueue;

static void setAutorun(bool on)
{
    Preferences prefs;
    prefs.begin("wasm", false);
    prefs.putBool("autorun", on);
    prefs.end();
}

bool wasm_runner_autorun()
{
    Preferences prefs;
    prefs.begin("wasm", true);
    bool on = prefs.getBool("autorun", true);
    prefs.end();
    return on;
}

static volatile bool running = false;

// Reads the module into PSRAM (falls back to internal RAM). wasm3 keeps pointing into
// this buffer until the module is freed, so the caller owns it for the whole run.
static uint8_t *readModule(size_t &size)
{
    File f = LittleFS.open(MODULE_PATH, "r");
    if (!f)
    {
        console_print("[wasm] no module uploaded yet\n");
        return nullptr;
    }
    size = f.size();
    auto *buf = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buf)
        buf = (uint8_t *)malloc(size);
    if (!buf || f.read(buf, size) != size)
    {
        console_print("[wasm] could not read module\n");
        free(buf);
        return nullptr;
    }
    return buf;
}

static void reportError(const char *what, M3Result res, IM3Runtime rt)
{
    if (res == m3Err_trapAbort && m3_StopFlag)
    {
        console_print("[wasm] stopped\n");
        return;
    }
    M3ErrorInfo info;
    m3_GetErrorInfo(rt, &info);
    if (info.message && info.message[0])
        console_printf("[wasm] %s: %s (%s)\n", what, res, info.message);
    else
        console_printf("[wasm] %s: %s\n", what, res);
}

static void runModule()
{
    size_t size = 0;
    uint8_t *bytes = readModule(size);
    if (!bytes)
        return;

    IM3Environment env = m3_NewEnvironment();
    IM3Runtime rt = env ? m3_NewRuntime(env, WASM_STACK_BYTES, nullptr) : nullptr;
    if (!rt)
    {
        console_print("[wasm] out of memory creating runtime\n");
    }
    else
    {
        IM3Module module = nullptr;
        M3Result res = m3_ParseModule(env, &module, bytes, size);
        if (res)
        {
            reportError("parse", res, rt);
        }
        else if ((res = m3_LoadModule(rt, module)))
        {
            reportError("load", res, rt);
            m3_FreeModule(module); // the runtime only owns it after a successful load
        }
        else
        {
            host_api_link(module);

            IM3Function fn = nullptr;
            if (m3_FindFunction(&fn, rt, "main") && m3_FindFunction(&fn, rt, "_start"))
            {
                console_print("[wasm] module exports neither main nor _start\n");
            }
            else
            {
                console_printf("[wasm] running (%u bytes)\n", (unsigned)size);
                res = m3_CallV(fn);
                if (res)
                    reportError("trap", res, rt);
                else
                    console_print("[wasm] finished\n");
            }
        }
    }

    if (rt)
        m3_FreeRuntime(rt);
    if (env)
        m3_FreeEnvironment(env);
    free(bytes);
}

static void runnerTask(void *)
{
    for (;;)
    {
        uint8_t cmd;
        xQueueReceive(runQueue, &cmd, portMAX_DELAY);

        m3_StopFlag = 0; // a stop requested while idle must not kill the next module
        host_api_stdin_clear();
        running = true;
        runModule();
        running = false;
    }
}

void wasm_runner_begin()
{
    // Length 1 + overwrite: several uploads in a row collapse into a single reload
    runQueue = xQueueCreate(1, sizeof(uint8_t));
    xTaskCreatePinnedToCore(runnerTask, "wasm", RUNNER_STACK_BYTES, nullptr, 2, nullptr, RUNNER_CORE);
}

void wasm_runner_run()
{
    uint8_t cmd = 1;
    setAutorun(true);
    if (running)
        m3_StopFlag = 1; // abort the current module; the task then picks up the reload
    xQueueOverwrite(runQueue, &cmd);
}

void wasm_runner_stop()
{
    setAutorun(false); // stay stopped across reboots until Run or a new upload
    if (running)
        m3_StopFlag = 1;
    else
        console_print("[wasm] nothing is running\n");
}

bool wasm_runner_is_running()
{
    return running;
}

void wasm_runner_input(const String &text)
{
    host_api_stdin_push(text.c_str(), text.length());
}
