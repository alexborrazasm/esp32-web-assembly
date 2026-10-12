#!/usr/bin/env python3
"""Tiny hand assembler for the example modules, so no WASM toolchain is needed.

    python3 examples/build_examples.py   # writes examples/*.wasm

Modules export `main` (no arguments) and import from "env" (see src/host_api.cpp).
"""
import pathlib


def uleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def sleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        done = (n == 0 and not b & 0x40) or (n == -1 and b & 0x40)
        out.append(b | (0 if done else 0x80))
        if done:
            return bytes(out)


def vec(items):
    return uleb(len(items)) + b"".join(items)


def name(s):
    return uleb(len(s)) + s.encode()


def section(sid, payload):
    return bytes([sid]) + uleb(len(payload)) + payload


def functype(params, results):
    return b"\x60" + vec([bytes([p]) for p in params]) + vec([bytes([r]) for r in results])


I32 = 0x7F


def const(n):
    return b"\x41" + sleb(n)


def call(idx):
    return b"\x10" + uleb(idx)


LOOP, END, BR0 = b"\x03\x40", b"\x0b", b"\x0c\x00"


def module(types, imports, body, data=()):
    """imports: [(field, type_index)]; main is the first defined function (no params)."""
    main_type = len(types)
    mod = b"\0asm\x01\0\0\0"
    mod += section(1, vec(types + [functype([], [])]))
    mod += section(2, vec([name("env") + name(f) + b"\x00" + uleb(t) for f, t in imports]))
    mod += section(3, vec([uleb(main_type)]))
    mod += section(5, vec([b"\x00\x01"]))  # one memory, min 1 page
    mod += section(7, vec([name("main") + b"\x00" + uleb(len(imports)), name("memory") + b"\x02\x00"]))
    code = b"\x00" + body + END  # no locals
    mod += section(10, vec([uleb(len(code)) + code]))
    if data:  # data = [(offset, bytes), ...]; offset 0 counts as a null pointer in wasm3
        mod += section(11, vec([b"\x00" + const(at) + END + uleb(len(b)) + b for at, b in data]))
    return mod


def hello():
    msg = b"Hello from WebAssembly!\n"
    body = const(16) + const(len(msg)) + call(0)
    return module([functype([I32, I32], [])], [("print", 0)], body, [(16, msg)])


ON_AT, ON_MSG = 16, b"LED on\n"
OFF_AT, OFF_MSG = 32, b"LED off\n"


def say(print_idx, at, msg):
    return const(at) + const(len(msg)) + call(print_idx)


def blink():
    """Blinks the onboard RGB LED (WS2812) green."""
    types = [functype([I32, I32, I32], []), functype([I32], []), functype([I32, I32], [])]
    imports = [("led_rgb", 0), ("delay_ms", 1), ("print", 2)]
    body = LOOP
    body += const(0) + const(32) + const(0) + call(0) + say(2, ON_AT, ON_MSG) + const(500) + call(1)
    body += const(0) + const(0) + const(0) + call(0) + say(2, OFF_AT, OFF_MSG) + const(500) + call(1)
    body += BR0 + END
    return module(types, imports, body, [(ON_AT, ON_MSG), (OFF_AT, OFF_MSG)])


def spin():
    """Prints once, then spins in a loop with no calls: the worst case for the Stop command."""
    msg = b"spinning, press Stop\n"
    body = const(16) + const(len(msg)) + call(0) + LOOP + BR0 + END
    return module([functype([I32, I32], [])], [("print", 0)], body, [(16, msg)])


out = pathlib.Path(__file__).parent
for fname, blob in [("hello", hello()), ("blink", blink()), ("spin", spin())]:
    (out / f"{fname}.wasm").write_bytes(blob)
    print(f"{fname}.wasm  {len(blob)} bytes")
