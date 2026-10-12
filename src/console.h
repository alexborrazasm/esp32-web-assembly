#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

// Virtual console: output goes to the USB Serial and to the /console WebSocket.
// A ring buffer keeps the history for clients that connect late.

using ConsoleInputHandler = std::function<void(const String &)>;

void console_begin(AsyncWebServer &server);
void console_write(const char *data, size_t len);
void console_print(const char *text);

// Empties the history and resets every connected terminal.
void console_clear();
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Called with the text the user types in the web terminal.
void console_on_input(ConsoleInputHandler handler);
