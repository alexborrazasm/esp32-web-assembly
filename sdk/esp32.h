// C API for modules running on the esp32-wasm board.
//
// Freestanding: no libc. Compile with examples/c/build.sh.
// Host implementation: src/host_api.cpp (keep both in sync).
#pragma once

#define ESP_IMPORT(name) __attribute__((import_module("env"), import_name(#name)))

// The runner calls the export named "main" (or "_start"), no arguments.
#define ESP_ENTRY __attribute__((export_name("main")))

// --- Console -------------------------------------------------------------

// Writes len bytes to the web console.
ESP_IMPORT(print) void print(const char *ptr, int len);

// Next character typed in the web console ('\n' ends a line), or -1 if none.
// Never blocks: poll it, with delay_ms() between polls.
ESP_IMPORT(read_char) int read_char(void);

// --- Time ----------------------------------------------------------------

// Milliseconds since the board booted.
ESP_IMPORT(millis) int millis(void);

// Sleeps. Returns early (the module is aborted) if Stop is pressed.
ESP_IMPORT(delay_ms) void delay_ms(int ms);

// --- GPIO ----------------------------------------------------------------
// Pins 19-20 (USB) and 26-37 (flash/PSRAM) are blocked and abort the module.

enum { INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2 };

ESP_IMPORT(gpio_mode) void gpio_mode(int pin, int mode);
ESP_IMPORT(gpio_write) void gpio_write(int pin, int value);
ESP_IMPORT(gpio_read) int gpio_read(int pin);

// Onboard RGB LED (WS2812), each channel 0-255.
ESP_IMPORT(led_rgb) void led_rgb(int r, int g, int b);

// --- Helpers (no libc, so provided here) -----------------------------------

static inline void print_str(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    print(s, n);
}

static inline void print_int(int v)
{
    char buf[12];
    int i = sizeof(buf);
    unsigned u = v < 0 ? 0u - (unsigned)v : (unsigned)v;
    do
    {
        buf[--i] = '0' + u % 10;
        u /= 10;
    } while (u);
    if (v < 0)
        buf[--i] = '-';
    print(buf + i, (int)sizeof(buf) - i);
}
