# esp32-wasm

Hot-swappable code for an ESP32-S3: upload a WebAssembly module from your browser and it runs on the board within a second, with a live console to talk to it. No reflashing.

```
  Browser                      WiFi                     ESP32-S3
 ┌──────────────────────┐              ┌────────────────────────────────┐
 │ drag & drop .wasm    │ POST /upload │ core 0: WiFi, web server,      │
 │ terminal + input     │ ───────────► │         console                │
 │ Run Stop Clear Reset │ WS /console  │                                │
 │                      │ ◄──────────► │ core 1: wasm3 runs your module │
 └──────────────────────┘              └────────────────────────────────┘
```

Write your program in C (or Rust, AssemblyScript, ...), compile it to `.wasm`, drop it on the page. The previous module is stopped and the new one starts. If the upload is not a valid module, the old one keeps running.

## Features

- Web UI served by the board itself: upload area, terminal (xterm.js), command line with history, Run / Stop / Clear / Reset buttons.
- Console in both directions: the module prints to the terminal and reads what you type.
- Runs on the second core, so heavy modules do not slow down WiFi or the web UI.
- Stop works on any module, even a bare `while (1) {}` (see [How Stop works](#how-stop-works)).
- WiFi setup portal (dark themed) when the saved network is not available.
- The last module is saved in flash and starts at boot, unless you stopped it.

## Hardware

Developed on an **ESP32-S3 DevKitC-1 N16R8** (16 MB flash, 8 MB octal PSRAM). Other S3 boards should work after adjusting `platformio.ini` (board, flash size, `memory_type`) and [partitions.csv](partitions.csv).

## Quick start

You need [PlatformIO](https://platformio.org/).

```sh
pio run -t upload      # firmware (also writes the partition table)
pio run -t uploadfs    # the web UI in data/ -> LittleFS
pio device monitor
```

1. On first boot the board opens a WiFi access point called **`esp32-wasm`**. Connect to it and pick your network in the portal. The credentials are stored in flash.
2. Open `http://esp32-wasm.local` (or the IP shown in the serial log).
3. Drop `examples/hello.wasm` on the page. You should see `Hello from WebAssembly!`.

If the board cannot join a network and nobody uses the portal for two minutes, it stays as an open access point with the same name.

## Writing modules

A module exports `main` (or `_start`) with no arguments and imports what it needs from the `env` module.

### In C

[sdk/esp32.h](sdk/esp32.h) declares the whole API. There is no libc: no `printf`, `malloc` or `argv`.

```c
#include "../../sdk/esp32.h"

ESP_ENTRY void run(void)
{
    for (;;)
    {
        led_rgb(0, 32, 0);
        print_str("on\n");
        delay_ms(500);
        led_rgb(0, 0, 0);
        print_str("off\n");
        delay_ms(500);
    }
}
```

Build with [examples/c/build.sh](examples/c/build.sh). It needs clang with the `wasm32` target and lld (`sudo dnf install clang lld`, or your distro's equivalent). The result lands in `examples/c/out/`.

Examples in [examples/c/](examples/c/): `hello.c`, `blink.c`, `echo.c` (reads what you type in the console).

### Without a toolchain

[examples/build_examples.py](examples/build_examples.py) is a tiny hand-written WASM assembler that generates `hello.wasm`, `blink.wasm` and `spin.wasm` with just Python. Handy to test the board before installing anything.

## Module API (`env` imports)

| Function | Signature | Description |
|---|---|---|
| `print` | `(ptr, len)` | Write `len` bytes to the console. |
| `read_char` | `() -> i32` | Next character typed in the console (a line ends with `\n`), or `-1` if none. Never blocks; poll it with `delay_ms`. |
| `millis` | `() -> i32` | Milliseconds since boot. |
| `delay_ms` | `(ms)` | Sleep. Aborts the module if Stop is pressed meanwhile. |
| `gpio_mode` | `(pin, mode)` | `0` input, `1` output, `2` input with pull-up. |
| `gpio_write` | `(pin, value)` | Set a pin high (non-zero) or low. |
| `gpio_read` | `(pin) -> i32` | Read a pin. |
| `led_rgb` | `(r, g, b)` | Onboard RGB LED (WS2812), 0-255 per channel. |

Notes:

- Pins 19-20 (USB) and 26-37 (flash and PSRAM) are blocked; using them traps the module.
- wasm3 treats memory offset 0 as a null pointer, so never put data there (C compilers don't).
- Limits: modules up to 4 MB, linear memory up to 64 pages (4 MB), 64 KB wasm stack, 256 bytes of pending stdin.

## HTTP API

| Route | Description |
|---|---|
| `GET /` | The web UI (from LittleFS). |
| `POST /upload` | Multipart upload of a `.wasm`. Validates the magic number, saves it and reloads. Replies `{"ok":true,"size":N}` or HTTP 400 with `{"ok":false,"error":"..."}`. |
| `GET /api/status` | `mode`, `ssid`, `ip`, `heap`, `psram`, `module`, `running`, `uptime`. |
| `POST /api/run` | Restart the saved module. |
| `POST /api/stop` | Stop the running module. It stays stopped across reboots until Run or a new upload. |
| `POST /api/clear` | Clear the console history and every open terminal. |
| `POST /api/reset` | Reboot the board. |
| `WS /console` | Server sends console text (plus the history on connect); client sends lines of text, delivered to the module's stdin. |

So you can deploy from a shell:

```sh
curl -F module=@examples/c/out/hello.wasm http://esp32-wasm.local/upload
```

## Project layout

```
src/
  main.cpp          setup: filesystem, WiFi, web server, runner
  wifi_setup.cpp    WiFiManager with dark portal, mDNS
  web.cpp           routes, upload handling
  console.cpp       console: serial + WebSocket + history ring buffer
  wasm_runner.cpp   wasm3 task on core 1, load / stop / reload
  host_api.cpp      the env.* imports
data/               web UI (xterm.js is vendored, gzipped, so it works offline)
sdk/esp32.h         C header for module authors
examples/           ready-to-use modules and the tools that build them
lib/Wasm3/          wasm3 with a small local patch (see PATCHES.md)
partitions.csv      16 MB layout: app, coredump, LittleFS
```

## Design notes

**Cores.** WiFi, lwIP, mDNS and the web server (AsyncTCP is pinned with `CONFIG_ASYNC_TCP_RUNNING_CORE=0`) run on core 0. The wasm runner is a FreeRTOS task pinned to core 1. Console output goes through a stream buffer drained by a core 0 task, so a chatty module never waits on the network (it drops output if the buffer overflows).

**How Stop works.** wasm3 cannot be interrupted from outside, so the vendored copy in `lib/Wasm3/` (upstream v0.9.0; the Arduino/PlatformIO package is stuck at 0.5.0) has a small patch: a `m3_StopFlag` that is checked on every function call and every loop iteration and aborts the module with a trap. That also stops a bare infinite loop, with no task killing and no leaked memory. Details in [lib/Wasm3/PATCHES.md](lib/Wasm3/PATCHES.md).

**Hot reload.** An upload is written to a temporary file and only replaces the saved module once it is complete and starts with the WASM magic number. Several uploads in a row collapse into one reload.

## Security and limitations

Modules run in the wasm3 sandbox. A module can only touch its own linear memory and call the `env` imports above, which validate their arguments. It cannot read or write the firmware, the WiFi credentials, NVS or the filesystem, and it cannot run native code.

What the sandbox does **not** limit:

- **CPU.** A module can spin forever. Stop aborts it, but there is no automatic time limit.
- **Memory.** Up to 4 MB of linear memory plus whatever wasm3 needs to compile it.
- **Pins.** A module can drive any allowed GPIO. A badly configured output can damage whatever you wired to it.
- **Console output.** A chatty module can flood the terminal. Excess output is dropped.

**There is no authentication.** Anyone on your network can upload a module or reboot the board. Use it on a network you trust.

If a module ever ignores Stop, press Stop anyway and then Reset: Stop records the stopped state in flash even when the module does not react, so it does not start again after the reboot. Run or a new upload starts it again.

## Credits

[wasm3](https://github.com/wasm3/wasm3) (MIT), [xterm.js](https://xtermjs.org/) (MIT), [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer), [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) and [WiFiManager](https://github.com/tzapu/WiFiManager).
