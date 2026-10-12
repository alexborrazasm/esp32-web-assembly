#pragma once

#include <wasm3.h>

// Links the "env" imports available to modules. Imports the module does not use are
// silently skipped.
void host_api_link(IM3Module module);

// Called by imports that block (delay): true once a stop was requested.
bool host_api_stop_requested();

// stdin plumbing, filled by wasm_runner_input() and read by `read_char`
void host_api_stdin_push(const char *data, size_t len);
void host_api_stdin_clear();
