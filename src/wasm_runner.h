#pragma once

#include <Arduino.h>

// Runs the uploaded WASM module on core 1, leaving core 0 to WiFi and the web server.
//
// Entry point: the module must export `main` or `_start` taking no arguments.
// Host imports live in module "env" (see host_api.cpp).

void wasm_runner_begin();

// (Re)load MODULE_PATH from LittleFS and run it. If a module is running it is
// stopped first. Safe to call from any task.
void wasm_runner_run();

// Ask the running module to stop. Returns immediately; the module aborts at its next
// function call or loop iteration.
void wasm_runner_stop();

bool wasm_runner_is_running();

// False if the module was stopped on purpose: it then stays stopped after a reboot.
bool wasm_runner_autorun();

// Feeds characters to the module's stdin (read by the `read_char` import).
void wasm_runner_input(const String &text);
