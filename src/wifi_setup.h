#pragma once

// Connects with the saved credentials; if that fails it opens the "esp32-wasm" AP
// with a config portal. Returns once there is a network (STA) or the portal times
// out, in which case it stays in AP mode.
void wifi_setup();
