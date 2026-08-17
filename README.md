# Beolab3500-Standalone

An ESP32 firmware that lets a Bang & Olufsen **Beolab 3500** satellite speaker activate a source without its real Master unit present. It listens on the B&O **MCL/PL "Datalink" bus** for the Beolab 3500's own source-select request and replies exactly the way a real Master (a Beocenter 2300, in this case) does — without that reply, the Beolab 3500 never switches on.

## Why

In a multiroom Beolink setup, a "link room" speaker like the Beolab 3500 talks to a central Master unit over a 2-wire bus. Point a remote at the Beolab 3500 directly and it sends a request onto the bus — but does nothing further until the Master answers. If the Master lives in a different room (or isn't there at all), the Beolab 3500 just sits idle. This project is a minimal stand-in for that one interaction: detect the request, send back the two frames a real Master would, nothing else.

## Hardware

- Board: ESP32 WROVER DevKit (`upesy_wrover`)
- **RX** — GPIO34, via a resistor divider from the bus data line (input-only pin, no pull needed)
- **TX** — GPIO25, driving an NPN transistor (base resistor + pull-down) that pulls the bus line low, plus a clamp diode
- The bus itself is 2 wires only: **Data** and **GND** (no separate supply pin needed — confirmed against a working real-Master connection)

The bus is wired-OR and active-low: idle is HIGH via a pull-up (supplied by whichever unit powers the bus, normally the Master), and a transmitting unit pulls the line LOW for a fixed strobe width per bit.

## Protocol notes (MCL-2 "Datalink '86")

- Five timing symbols: t1=3.125ms, t2=6.250ms, t3=9.375ms (data bits), t4=12.500ms (Stop), t5=15.625ms (Start)
- A frame's Start is preceded by **two AGC-priming t1 pulses** (`t1, t1, t5`) — a real receiver's analog front-end needs these to lock onto the signal, even though a pure digital receiver decodes fine without them. Missing this was the first of two bugs that kept the Beolab 3500 from activating during development.
- Bits are differentially encoded: which timing symbol represents a `0` or `1` depends on the previous bit.
- Frame = `Start | Format(3 bit) | Address(to)(5 bit) | Address(from)(4 bit) | Data | Stop`.
- The Beolab 3500's own request is a short frame: `addrFrom=12` (its bus address), `addrTo=0`, and `data` is a Beo4 remote key code masked to 5 bits (`BEO_CMD_XXX & 0x1F`).

## How it works

1. `MclBusReader` continuously decodes bus traffic and hands complete frames to `loop()`.
2. `PLData` parses each frame's header and, if it matches the Beolab 3500's short notify pattern, resolves which source was requested (`device = data + 192`, cross-checked against the full Beo4 command table — see source comments for how that formula was derived).
3. `loop()` replies with two frames, exactly as captured off a real Beocenter 2300: a Sound frame and a SelectSource frame for the requested device.
4. One GPIO per audio source is also driven HIGH for whichever source is currently active (`SOURCE_PINS[]` in `main.cpp`) — meant for a separate relay/routing board to pick up which physical audio input should be live, with no protocol knowledge needed on that side.

## Status

- TV, Radio, and CD have been directly verified against real hardware. The other 10 sources follow the same derived formula but haven't been individually confirmed on a Beolab 3500 yet.
- An unprompted reply at ESP32 boot (with no prior request from the Beolab 3500) does **not** activate it — the Beolab 3500 has to initiate first.
- The Beolab 3500 occasionally sends a notify from a different sender address (11) just before its own (12) for the same button press; harmless (filtered out) but not understood.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run              # build
pio run -t upload    # flash
pio device monitor    # serial log (115200 baud)
```

## Project structure

- `src/main.cpp` — wiring: read a frame, filter for the Beolab 3500's request, reply, drive the active source pin
- `src/MclBusReader.*` — RMT-based bus capture and pulse-to-bit decoding
- `src/MclBusWriter.*` — bit-to-pulse encoding and transmission
- `src/PLData.*` — frame parsing/building (header fields, device mapping, Sound/SelectSource frame construction)

## Related

- [`aanban/esp32_beo4`](https://github.com/aanban/esp32_beo4) — Beo4 IR remote library (used here for the confirmed `BEO_CMD_*` command table)
- A sister project, `BuOPowerlink`, provided an independent second bus reader used throughout development to confirm signal issues were real and not specific to the Beolab 3500

## License

Not yet decided.
