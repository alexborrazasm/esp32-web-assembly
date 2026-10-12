# Wasm3 (vendored) and local patches

Source: <https://github.com/wasm3/wasm3> tag **v0.9.0**, MIT licensed (see LICENSE).
The PlatformIO / Arduino package is stuck at 0.5.0 (2021), so the upstream `source/` directory is
copied here instead. Not copied: the WASI, libc and tracer API files (`m3_api_*`), not needed here.

To update: replace `src/` with the new `source/` contents and reapply the three patches below.

## Build flags (platformio.ini)

- `d_m3MaxLinearMemoryPages=64`: cap module memory at 4 MB.

## Patches: cooperative stop

Lets another task abort a running module.

- `m3_core.c`: `volatile int m3_StopFlag`; `m3_Yield()` returns `m3Err_trapAbort` when set
  (the interpreter calls it on every function call).
- `wasm3.h`: `extern` declaration of `m3_StopFlag`.
- `m3_exec.h`: `op_Loop` checks the flag on every iteration, so a `while(1){}` with no
  calls can be stopped too.
