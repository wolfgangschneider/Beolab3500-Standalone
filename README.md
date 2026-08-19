# Beolab3500-Standalone

> **⚠️ Only verified against the Beolab 3500 Mk1.** Other Mk revisions may use different bus electrical characteristics or protocol details — do not assume this works unmodified on anything else.

An ESP32 firmware that lets a Bang & Olufsen **Beolab 3500** satellite speaker activate a source without its real Master unit present. It listens on the B&O **MCL/PL "Datalink" bus** for the Beolab 3500's own source-select request and replies exactly the way a real Master (a Beocenter 2300, in this case) does — without that reply, the Beolab 3500 never switches on.

## The basic idea

When you press a source key on the Beolab 3500's own remote:

1. **Beolab 3500 → bus**: a 17-bit notify frame naming the source (Radio, here):

   ```
   00000000110000001
   ```
   Format(3)=`000`, Address(to)(5)=`00000`, Address(from)(4)=`1100` (=12, its own bus address), Data(5)=`00001` (=1, Radio's Beo4 key code)

2. **This board → bus**: two frames, exactly as a real Master (Beocenter 2300) sends them:

   Sound (47 bits):
   ```
   00110011010011101011000000001111000001010001000
   ```
   SelectSource (48 bits):
   ```
   001110111100000101100000000001000000001000000000
   ```

Without step 2, the Beolab 3500 receives its own notify but never actually activates the source. See [Protocol notes](#protocol-notes-mcl-2-datalink-86) and [How it works](#how-it-works) below for the full field-level breakdown.

## Why

In a multiroom Beolink setup, a "link room" speaker like the Beolab 3500 talks to a central Master unit over a 2-wire bus. Point a remote at the Beolab 3500 directly and it sends a request onto the bus — but does nothing further until the Master answers. If the Master lives in a different room (or isn't there at all), the Beolab 3500 just sits idle. This project is a minimal stand-in for that one interaction: detect the request, send back the two frames a real Master would, nothing else.

## Hardware

- Board: ESP32 WROVER DevKit (`upesy_wrover`)
- **RX** — GPIO34, via a resistor divider from the bus data line (input-only pin, no pull needed)
- **TX** — GPIO25, driving an NPN transistor (BC847) that pulls the bus line low; base resistor from GPIO25, base pull-down to GND
- The bus itself is 2 wires only: **Data** and **GND** (no separate supply pin needed — confirmed against a working real-Master connection)

The bus is wired-OR and active-low: idle is HIGH via a pull-up (normally supplied by the Master, per the MCL-2 spec's 4.0–5.5V output-high range), and a transmitting unit pulls the line LOW for a fixed strobe width per bit. Since this project's whole point is running without a Master present, **this board supplies that pull-up itself** — from an external 5V supply, not the ESP32's own 3.3V GPIO — see the schematic below.

### MCL DIN8 connector pinout

The Beolink MCL cable carries more than just the bus — this project only taps 2 of its 7 pins (Data + Shield/GND, see [bus interface](#schematic-bus-interface) above). The remaining pins carry stereo audio + a DC supply, documented here for whoever wires up the audio-switch side (see [per-source select outputs](#schematic-per-source-select-outputs) above):

```
Pin 1 ─── Yellow (L hot)      ─────────────────────────► Audio jack L, tip

Pin 4 ─── Green  (R hot)      ─────────────────────────► Audio jack R, tip

                                                   ┌───► Audio jack L+R, sleeve
Pin 3 ─── Grey   (L gnd)     ──┐                   │
                               ├───────────────────┤
Pin 5 ─── Brown  (R gnd)     ──┘                   │
                                                   └───► ESP32 GND

Pin 2 ─── Pink   (DC 7.5–8.5V) ────────────────────────► not used

Pin 6 ─── White  (Data)       ─────────────────────────► see Schematic (bus interface)

Pin 7 ─── Shield (GND)        ─────────────────────────► see Schematic (bus interface)
```

### Schematic (bus interface)

```
                                    +5V  ESP32
                                     │   
                                   [2K2]  pull-up since there's no Master
                                     │
                    MCL/PL Bus — Data ─────┬──────────────────────────────┬───────► Beolab 3500 (pin 6)
                                           │                              │
                                          [R3]                            │ (collector)
                                          10k                           ┌─┴─┐
                                           │                            │   │
                                GPIO34 ────┤                            │Q1 │  BC847 (NPN)
                                (RX)       │                  (base)    │   │
                                         [R4]              ┌────────────┤   │
                                         15k               │            └─┬─┘
                                           │              [R1]            │ (emitter)
                                          GND             2k2             │
                                                           │             GND
                                GPIO25 ────────────────────┤
                                (TX)                       │
                                                          [R2]
                                                          10k
                                                           │
                                                          GND

                    MCL/PL Bus — GND  ───────────────────────────────────────────► Beolab 3500 (pin 7)
```

### Schematic (per-source select outputs)

One GPIO per audio source (`SOURCE_PINS[]` in `main.cpp`), driven HIGH for whichever source is currently active and LOW for all others. A separate board reads these directly — no decoding needed on its side:

```
ESP32 — wrover  ⚠️ work in progress, will change
┌───────────────────────────┐
│  GPIO4  (TV)      ●───────┼──► HIGH while TV is the active source
│  GPIO5  (Radio)   ●───────┼──► HIGH while Radio is the active source
│  GPIO17 (PC)      ●───────┼──► ...             ──► to a separate audio-switch board
│  GPIO19 (CD)      ●───────┼──► ...                 (not part of this project)
│  GPIO22 (Phono)   ●───────┼──► ...
└───────────────────────────┘
```

Currently active in `SOURCE_PINS[]`; the rest of the 13 sources (V.Aux, A.Aux, V.Tape, DVD, Sat, A.Tape, A.Tape2, CD2) are commented out for now, not disconnected for any technical reason — just trimmed down while testing.

Pin numbers are placeholders for the current dev board (`upesy_wrover`) and free to reassign — see [Status](#status).

### Schematic (navigation key outputs) could be used for Bluetooth navigation

Same pattern as the per-source outputs above, but for the Left/Right/Stop keys (`KEY_PINS[]`, `setActiveKeyPin()`), intercepted *before* the source-select mapping — Left(18)/Right(20) would otherwise collide with real device numbers once `+192` is applied (18+192=210=CD, 20+192=212=A.Tape2). Idea: drive a Bluetooth controller's Next/Prev/Pause. Not wired up yet, and unlike the sources, these three key values are only derived from the same `&0x1F` formula — not individually confirmed against real Beolab 3500 hardware:

```
ESP32 - wrover⚠️ work in progress, will change
┌───────────────────────────┐
│  GPIO32 (Left)   ●────────┼──► HIGH while Left is pressed   (-> Bluetooth Prev?)
│  GPIO33 (Right)  ●────────┼──► HIGH while Right is pressed  (-> Bluetooth Next?)
│  GPIO2  (Stop)   ●────────┼──► HIGH while Stop is pressed   (-> Bluetooth Pause?)
└───────────────────────────┘
```

## Protocol notes (MCL-2 "Datalink '86")

- Five timing symbols: t1=3.125ms, t2=6.250ms, t3=9.375ms (data bits), t4=12.500ms (Stop), t5=15.625ms (Start)
- A frame's Start is preceded by **two AGC-priming t1 pulses** (`t1, t1, t5`) — a real receiver's analog front-end needs these to lock onto the signal, even though a pure digital receiver decodes fine without them. Missing this was the first of two bugs that kept the Beolab 3500 from activating during development.
- Bits are differentially encoded: which timing symbol represents a `0` or `1` depends on the previous bit.
- Frame = `Start | Format(3 bit) | Address(to)(5 bit) | Address(from)(4 bit) | Data | Stop`.
- The Beolab 3500's own request is a short frame: `addrFrom=12` (its bus address), `addrTo=0`, and `data` is a Beo4 remote key code masked to 5 bits (`BEO_CMD_XXX & 0x1F`).

## How it works

1. `PLBusReader` continuously decodes bus traffic and hands complete frames to `loop()`.
2. `PLData` parses each frame's header and, if it matches the Beolab 3500's short notify pattern, resolves which source was requested (`device = data + 192`, cross-checked against the full Beo4 command table — see source comments for how that formula was derived).
3. Left/Right/Stop are intercepted here and just drive a `KEY_PINS[]` output (see [navigation key outputs](#schematic-navigation-key-outputs) above) instead of a source reply.
4. `loop()` replies with two frames, exactly as captured off a real Beocenter 2300: a Sound frame and a SelectSource frame for the requested device.
5. One GPIO per audio source is also driven HIGH for whichever source is currently active (`SOURCE_PINS[]` in `main.cpp`) — meant for a separate relay/routing board to pick up which physical audio input should be live, with no protocol knowledge needed on that side.

## Debug / testing over Serial

With no Beolab 3500 on the bus, type a line into the serial monitor (115200 baud) to trigger things manually:

- A source name (`radio`, `tv`, `cd`, `dvd`, `sat`, `pc`, `a.tape`, `a.tape2`, `cd2`, `v.aux`, `a.aux`, `v.tape`, `phono`) or a bare device number (e.g. `193`) — sends the full reply for that source, as if the Beolab 3500 had just requested it.
- `<type> <subType> <value>` (e.g. `78 3 68`) — sends a one-off Sound frame with those exact field values, for figuring out what each field does (see [Status](#status) — most of it is still undocumented).

## Status

- TV, Radio, and CD have been directly verified against real hardware. The other 10 sources follow the same derived formula but haven't been individually confirmed on a Beolab 3500 yet.
- An unprompted reply at ESP32 boot (with no prior request from the Beolab 3500) does **not** activate it — the Beolab 3500 has to initiate first.
- The Beolab 3500 occasionally sends a notify from a different sender address (11) just before its own (12) for the same button press; harmless (filtered out) but not understood.
- `SOURCE_PINS[]` is expected to change — 13 dedicated pins for 13 sources is probably more than needed, and the source list will likely get pruned/reworked rather than staying 1:1.
- The Sound frame's fields are mostly unexplored: `type=78` looks volume-related (real Master traffic seen with `type=78 subType=4`, value changing by a fixed step per Vol+/Vol- press — but `subType=4` isn't documented anywhere), and the known `subType=135` (VOLUME) hasn't been tried yet. Use the [debug Serial command](#debug--testing-over-serial) above to explore.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run              # build
pio run -t upload    # flash
pio device monitor    # serial log (115200 baud)
```

## Project structure

- `src/main.cpp` — wiring: read a frame, filter for the Beolab 3500's request, reply, drive the active source pin
- `src/PLBusReader.*` — RMT-based bus capture and pulse-to-bit decoding
- `src/PLBusWriter.*` — bit-to-pulse encoding and transmission
- `src/PLData.*` — frame parsing/building (header fields, device mapping, Sound/SelectSource frame construction)

## Related

- [`aanban/esp32_beo4`](https://github.com/aanban/esp32_beo4) — Beo4 IR remote library (used here for the confirmed `BEO_CMD_*` command table)
- A sister project, `BeoPowerlinkDisplay`, provided an independent second bus reader used throughout development to confirm signal issues were real and not specific to the Beolab 3500

## License

Not yet decided.
