#include "host_api.h"

#include <Arduino.h>

#include "console.h"

static QueueHandle_t stdinQueue;

static void ensureStdin()
{
    if (!stdinQueue)
        stdinQueue = xQueueCreate(256, sizeof(uint8_t));
}

void host_api_stdin_push(const char *data, size_t len)
{
    ensureStdin();
    for (size_t i = 0; i < len; i++)
        xQueueSend(stdinQueue, &data[i], 0); // drops input if the module is not reading
}

void host_api_stdin_clear()
{
    ensureStdin();
    xQueueReset(stdinQueue);
}

bool host_api_stop_requested()
{
    return m3_StopFlag;
}

// Flash (26-32), octal PSRAM (33-37) and native USB (19-20) pins are off limits
static bool pinAllowed(int32_t pin)
{
    if (pin < 0 || pin > 48)
        return false;
    if (pin >= 26 && pin <= 37)
        return false;
    return pin != 19 && pin != 20;
}

// void print(const char *ptr, int len)
m3ApiRawFunction(env_print)
{
    m3ApiGetArgMem(const char *, ptr);
    m3ApiGetArg(int32_t, len);
    if (len < 0)
        m3ApiTrap(m3Err_trapOutOfBoundsMemoryAccess);
    m3ApiCheckMem(ptr, len);
    console_write(ptr, len);
    m3ApiSuccess();
}

// int millis()
m3ApiRawFunction(env_millis)
{
    m3ApiReturnType(int32_t);
    m3ApiReturn((int32_t)millis());
}

// void delay_ms(int ms) - sleeps in small slices so a stop request is honoured
m3ApiRawFunction(env_delay_ms)
{
    m3ApiGetArg(int32_t, ms);
    uint32_t end = millis() + (ms > 0 ? ms : 0);
    while (!m3_StopFlag)
    {
        int32_t left = (int32_t)(end - millis());
        if (left <= 0)
            break;
        vTaskDelay(left > 20 ? pdMS_TO_TICKS(20) : 1);
    }
    if (m3_StopFlag)
        m3ApiTrap(m3Err_trapAbort);
    m3ApiSuccess();
}

// int read_char() - next stdin byte, or -1 if there is none (non-blocking)
m3ApiRawFunction(env_read_char)
{
    m3ApiReturnType(int32_t);
    uint8_t c;
    ensureStdin();
    m3ApiReturn(xQueueReceive(stdinQueue, &c, 0) == pdTRUE ? (int32_t)c : -1);
}

// void gpio_mode(int pin, int mode)   mode: 0 input, 1 output, 2 input with pull-up
m3ApiRawFunction(env_gpio_mode)
{
    m3ApiGetArg(int32_t, pin);
    m3ApiGetArg(int32_t, mode);
    if (!pinAllowed(pin) || mode < 0 || mode > 2)
        m3ApiTrap("invalid gpio_mode arguments");
    pinMode(pin, mode == 1 ? OUTPUT : mode == 2 ? INPUT_PULLUP
                                                : INPUT);
    m3ApiSuccess();
}

// void gpio_write(int pin, int value)
m3ApiRawFunction(env_gpio_write)
{
    m3ApiGetArg(int32_t, pin);
    m3ApiGetArg(int32_t, value);
    if (!pinAllowed(pin))
        m3ApiTrap("invalid gpio pin");
    digitalWrite(pin, value ? HIGH : LOW);
    m3ApiSuccess();
}

// void led_rgb(int r, int g, int b)   onboard WS2812, values 0-255
m3ApiRawFunction(env_led_rgb)
{
    m3ApiGetArg(int32_t, r);
    m3ApiGetArg(int32_t, g);
    m3ApiGetArg(int32_t, b);
    neopixelWrite(RGB_BUILTIN, constrain(r, 0, 255), constrain(g, 0, 255), constrain(b, 0, 255));
    m3ApiSuccess();
}

// int gpio_read(int pin)
m3ApiRawFunction(env_gpio_read)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, pin);
    if (!pinAllowed(pin))
        m3ApiTrap("invalid gpio pin");
    m3ApiReturn(digitalRead(pin));
}

void host_api_link(IM3Module module)
{
    // m3Err_functionLookupFailed just means the module does not import that function
    m3_LinkRawFunction(module, "env", "print", "v(*i)", env_print);
    m3_LinkRawFunction(module, "env", "millis", "i()", env_millis);
    m3_LinkRawFunction(module, "env", "delay_ms", "v(i)", env_delay_ms);
    m3_LinkRawFunction(module, "env", "read_char", "i()", env_read_char);
    m3_LinkRawFunction(module, "env", "gpio_mode", "v(ii)", env_gpio_mode);
    m3_LinkRawFunction(module, "env", "gpio_write", "v(ii)", env_gpio_write);
    m3_LinkRawFunction(module, "env", "gpio_read", "i(i)", env_gpio_read);
    m3_LinkRawFunction(module, "env", "led_rgb", "v(iii)", env_led_rgb);
}
