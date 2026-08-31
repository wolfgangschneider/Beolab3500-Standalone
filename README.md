# Beolab3500-Standalone

An ESP32 firmware that lets a Bang & Olufsen **Beolab 3500** satellite speaker activate a source without its real Master unit present. Unlike the Menu-0-0-4 trick, this stays permanent even after a power loss — and on Mk I it also gives more possibilities, like selecting different sources.

## Switching to PL/MCL mode

Before this project can talk to a Beolab 3500, the speaker itself has to be switched into PL/MCL bus mode via its own menu:

- **Mk II**: Menu, Menu, 0, 9, Go, then Up/Down to (off), Go.
- **Mk I**: Menu, 0, 9, Go, then Up/Down to Off, Go.  (not sure if realy needed)



## Beolab 3500 MKI
Testd on SW 1.1
### The basic idea

When you press a source key on the Beolab 3500's own remote:

1. **Beolab 3500 → bus**: a 17-bit notify frame naming the source (Radio, here):

   ```
   00000000110000001
   ```
   Format(3)=`000`, Address(to)(5)=`00000`, Address(from)(4)=`1100` (=12, its own bus address), Data(5)=`00001` (=1, Radio's Beo4 key code)

2. **This board → bus**: a Sound and a SelectSource frame, exactly as a real Master (Beocenter 2300) sends them:

   Sound (47 bits):
   ```
   00110011010011101011000000001111000001010001000
   ```
   SelectSource (48 bits):
   ```
   001110111100000101100000000001000000001000000000
   ```

Without step 2, the Beolab 3500 receives its own notify but never actually activates the source. See [Protocol notes](#protocol-notes-mcl-2-datalink-86) and [How it works](#how-it-works) below for the full field-level breakdown.





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

       MCL/PL Bus — Data       ──────┬─────┬──────────────────────────────┬─────────────────────► Beolab 3500 (pin 6)
                                     │     │                              │
                                   [R5]    │                              │
  pull-up since there's no Master  [2K2]   │                              │
                                     │     │                              │
                                   +5V     │                              │
                                          [R3]                            │
                                          10k                             │
                                           │                              │
                                GPIO1  ────┤                              │
                                (RX)       │                              │ 
                                          [R4]                            │
                                          22k                             │ (collector) 
                                           │                            ┌─┴─┐   
                                          GND                           │   │
                                                                (base)  | T1| BC847 (NPN)
                                GPIO3  ─────[R1] 2k2───────┬────────────┤   │
                                 (TX)                      │            └─┬─┘
                                                           │              │ (emitter)
                                                          [R2]            │
                                                          10k             │
                                                           │              │
                                                          GND            GND

                    ESP32 — GND   ───────────────────────────────────────────► Beolab 3500 (pin 7)
 
 
 Beolab MK2 - GPIO1 (RX) line above isn't needed, MK2 never calls reader.begin()

                    GPIO5  (Mute)          ─────────────────────────────────────────► Beolab 3500 MKII (pin 4)
                    GPIO43 (MK2 detection) ◄───────────────────────────────────────── ESP32 +3V3  (wire this on MK2 boards so MK2_DETECTED reads HIGH)
                    GPIO9  (External Force Mute) ◄──────────────────────────────────  external mute source (HIGH = mute)
```

Pin numbers above are for `standalone_m5_stamp_S3` (the active `default_envs`); `standalone_esp32_wrover` uses different physical pins for the same circuit (GPIO34/25/26 for RX/TX/Mute - see the `#ifdef` block at the top of `src/main-standalone.cpp`).

All named pins on `standalone_m5_stamp_S3`, and what each one does in MK1 vs MK2 (`blVersion` is now auto-detected at boot via `MK2_DETECTED`, not hand-set - see `src/main-standalone.cpp`'s `setup()`):

| Pin | MK1 | MK2 |
|---|---|---|
| GPIO1  | `MCL_RX_PIN` - reads the bus | unused (`reader.begin()` not called) |
| GPIO3  | `MCL_TX_PIN` - sends on the bus | `MCL_TX_PIN` - sends on the bus |
| GPIO5  | `KEY_PIN_LEFT` (nav key input) | `MK2_MUTE_PIN` (mute output to Beolab pin 4) |
| GPIO7  | `KEY_PIN_RIGHT` (nav key input) | unused |
| GPIO9  | `KEY_PIN_STOP` (nav key input) | `MK2_BL_MUTE_PIN` (reads an external (BL) mute signal) |
| GPIO43 | `SOURCE_PINS` Radio output *(after boot)* | `MK2_DETECTED` (read once at boot to pick MK1 vs MK2) |
| GPIO44 | `SOURCE_PINS` TV output | unused |

GPIO5/9/43 are deliberately shared between an MK1-only and an MK2-only purpose - safe because `blVersion` is fixed for the whole run (decided once at boot from GPIO43) and the MK1-only code (`beginKeyPins()`/`pressKey()`/`beginSourcePins()`) vs MK2-only code (the mute-mirror block in `loop()`) never both run in the same boot. GPIO43 is the odd one out: it's read once as `MK2_DETECTED` *before* `blVersion` is known, then - only on MK1 - repurposed as the Radio source-select output for the rest of that run.


### Schematic (per-source select outputs)

One GPIO per audio source (`SOURCE_PINS[]` in `src/common/GpioOutputs.cpp`), driven HIGH for whichever source is currently active and LOW for all others. A separate board reads these directly — no decoding needed on its side:

```
ESP32 — standalone_m5_stamp_S3  ⚠️ work in progress, will change
┌───────────────────────────┐
│  GPIO44 (TV)      ●───────┼──► HIGH while TV is the active source
│  GPIO43 (Radio)   ●───────┼──► HIGH while Radio is the active source
└───────────────────────────┘
```

Currently active in `SOURCE_PINS[]` (MK1 only); PC and CD are commented out for now (GPIO17/19 aren't broken out on the Stamp S3 module - see the pin table above), along with the rest of the 13 sources (V.Aux, A.Aux, V.Tape, DVD, Sat, Phono, A.Tape, A.Tape2, CD2), not disconnected for any technical reason - just trimmed down while testing.

Pin numbers are placeholders and free to reassign - see `src/common/GpioOutputs.cpp` for the current board-specific list.

### Schematic (navigation key outputs) could be used for Bluetooth navigation

Same pattern as the per-source outputs above, but for the Left/Right/Stop keys (`KEY_PINS[]` in `src/common/GpioOutputs.cpp`, dispatched from `GpioOutputs::handleNavKeys()`), intercepted *before* the source-select mapping — Left(18)/Right(20) would otherwise collide with real device numbers once `+192` is applied (18+192=210=CD, 20+192=212=A.Tape2). Idea: drive a Bluetooth controller's Next/Prev/Pause. Not wired up yet, and unlike the sources, these three key values are only derived from the same `&0x1F` formula — not individually confirmed against real Beolab 3500 hardware:

```
ESP32 - standalone_m5_stamp_S3 ⚠️ work in progress, will change
┌───────────────────────────┐
│  GPIO5  (Left)   ●────────┼──► HIGH while Left is pressed   (-> Bluetooth Prev?)
│  GPIO7  (Right)  ●────────┼──► HIGH while Right is pressed  (-> Bluetooth Next?)
│  GPIO9  (Stop)   ●────────┼──► HIGH while Stop is pressed   (-> Bluetooth Pause?)
└───────────────────────────┘
```

GPIO5/9 are shared with `MK2_MUTE_PIN`/`MK2_BL_MUTE_PIN` (MK1-only vs MK2-only, see the pin table above) - not a typo.

### Protocol notes (MCL-2 "Datalink '86")

[Bang_and_Olufsen_MCL-2_service_manual.pdf](Bang_and_Olufsen_MCL-2_service_manual.pdf)



## Beolab 3500 Mk II

Tested with SW 3.33


When the Beolab 3500 Mk II is in PL mode, it behaves like a normal BL speaker (BL6000, BL8000): no IR reception, no SelectSource, and **also no Vol+/Vol-**. A speaker like that having its own display doesn't really make sense in the first place.

> ⚠️ **Big warning**: PL-level audio signals are much lower than a typical Line In / audio jack level. Since the Beolab 3500 Mk II's volume can't be controlled over PL (see above), feeding it a normal Line-In-level signal directly risks destroying it. Attenuate the input first — a voltage divider, or a level control on the source device — before connecting it.

## The basic idea


### Without display

Simple, two things needed — tie **PL4** to **+5V**, and send `0011000111100111111100000000100` on **PL6 / PL7**.

### With display

It gets more complicated — two additional requirements:
1. After a frame's normal Stop (t4), one more t1 pulse (3.125ms) - applies to every MK2 command, not just the init sequence.
2. Mute (`MK2_MUTE_PIN`, GPIO26 in `main-standalone.cpp`) must only go HIGH *after* that init sequence (`0011000111100111111100000000100`) has finished sending — it has to stay LOW for the whole duration of the sequence.

### Schematic (bus interface)
see MK I

### PL DIN8 connector pinout
The Beolink MCL cable carries more than just the bus — this project only taps 2 of its 7 pins (Data + Shield/GND, see bus interface above). The remaining pins carry stereo audio + a DC supply, documented here for whoever wires up the audio-switch side (see per-source select outputs above):

Pin 1 ─── Greay  (power)   ─────────────────────────► not used

Pin 2 ─── Blue  (GND)     ─────────────────────────► Audio jack GND, tip

Pin 3 ─── Braun   (Left)  ─────────────────────────► Audio jack Left, tip
                               
Pin 4 ─── Yellow  (Mute)  ─────────────────────────► See Schematic (bus interface)                │

Pin 5 ─── Green   (right) ─────────────────────────► Audio jack Left, tip

Pin 6 ─── White  (Data)   ─────────────────────────► see Schematic (bus interface)

Pin 7 ─── Shield (GND)    ─────────────────────────► see Schematic (bus interface)




## Project structure

This repo actually holds two separate firmwares, picked per `platformio.ini` env via `build_src_filter` (only one `setup()`/`loop()` pair ever lands in a given build):

- **Beolab3500-Standalone** (this section) — `src/main-standalone.cpp` + all of `src/common/`, envs `standalone_m5_stamp_S3`/`standalone_esp32_wrover`.
- **[Beolab3500-PL2PL](#beolab3500-pl2pl)** — `src/main-pl2pl.cpp`, uses only a plain `common/BusWriter` (not `MclBusWriter`/`PlBusWriter`/`MclData`), env `pl2pl_m5_stamp_S3`.

Both Beolab 3500 revisions share one bus implementation (confirmed identical wire protocol) and one entry point:

- `src/main-standalone.cpp` — auto-detects MK1 vs MK2 via GPIO at boot and points a single `BusWriter *writer` at the matching subclass:
  - **MK1**: fully automatic — read an MCL/PL notify frame, filter for the Beolab 3500's request, reply, drive the active source pin.
  - **MK2**: no automatic flow (BL3500 Mk2 doesn't send anything of its own onto the bus — it's a passive speaker, all traffic originates from the real Master) — `setup()` sends a built power-on sequence once at boot (`writer->sendInit()`), and `SerialDebugCommands`'s Serial commands are otherwise the only way to send anything: `init` and `vol <value>` (MK2 only), plus the shared `<source name> [track]` command also used by MK1.
- `src/common/`:
  - `BL3500Version.hpp` — the `MK1`/`MK2` enum, decided once at boot and used to pick the writer subclass and gate MK1-/MK2-only code
  - `SerialDebugCommands.*` — the Serial command-line handler (`init`/`vol <value>`/`<source name> [track]`), polled from `main-standalone.cpp`'s `loop()`
  - `BusReader.*` — RMT-based bus capture and pulse-to-bit decoding, shared by both revisions (MK1 only calls `begin()`/`poll()` on it - MK2 has nothing to read)
  - `BusWriter.*` — base class: bit-to-pulse encoding/transmission (`begin`/`sendFrame`/`pulse`, identical for both revisions) plus the `sendSource`/`sendVol`/`sendInit` interface, virtual with harmless "not available" defaults (not pure virtual - keeps `BusWriter` itself concretely instantiable, which Beolab3500-PL2PL below relies on), implemented for real by:
    - `MclBusWriter.*` — MK1's real frame content
    - `PlBusWriter.*` — MK2's real frame content (including its extra trailing pulse - see [Beolab 3500 Mk II](#beolab-3500-mk-ii))
  - `MclData.*` — frame parsing/building (header fields, device mapping, Sound/SelectSource frame construction) — parsing is confirmed against MK1 only; the builders don't take a `BL3500Version` (each subclass above just calls them with different arguments)
  - `GpioOutputs.*` — the downstream per-source and navigation-key GPIO outputs, shared since they're hardware-side, not protocol-specific (MK1 only for now). `handleNavKeys()` recognizes Left/Right/Stop notify frames and drives the matching `KEY_PINS[]` output instead of a source reply (see [navigation key outputs](#schematic-navigation-key-outputs) below)

## How it works

1. `BusReader` continuously decodes bus traffic and hands complete frames to `loop()`.
2. `MclData` parses each frame's header and, if it matches the Beolab 3500's short notify pattern, resolves which source was requested (`device = data + 192`, cross-checked against the full Beo4 command table — see source comments for how that formula was derived).
3. `GpioOutputs::handleNavKeys()` intercepts Left/Right/Stop and drives a `KEY_PINS[]` output (see [navigation key outputs](#schematic-navigation-key-outputs) above) instead of a source reply.
4. `loop()` calls `writer->sendSource(device, track)` (see `common/MclBusWriter.cpp`), which replies with a SelectSource frame for the requested device, as a real Beocenter 2300 would.
5. One GPIO per audio source is also driven HIGH for whichever source is currently active (`GpioOutputs::setActiveSourcePin()`, called from `main-standalone.cpp` right after `writer->sendSource()`) — meant for a separate relay/routing board to pick up which physical audio input should be live, with no protocol knowledge needed on that side.


## Beolab3500-PL2PL

A second, much simpler firmware in this same repo (`src/main-pl2pl.cpp`, env `pl2pl_m5_stamp_S3`) — a minimal trigger board for a Beolab 3500 MKII on the B&O PL bus. No RX, no MK1 support, no other commands - just one job:

- **GPIO5** (input, pulldown) - external trigger. On the LOW→HIGH edge, sends the init frame once (not repeated while held HIGH).
- **GPIO1** (output) - drives the bus transistor, same electrical interface as Beolab3500-Standalone above (see [Schematic (bus interface)](#schematic-bus-interface)).

Talks to a plain `common/BusWriter` directly (not `PlBusWriter`/`MclData` - it never does source selection or volume, so the real implementations and MclData dependency those pull in aren't needed) - just `begin()`, then `sendFrame()` + a trailing `pulse(1)` for the one fixed init frame. ESP32 only.


## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — free to use, modify, and share (with attribution, same license for derivatives), non-commercial only. See [`LICENSE`](LICENSE).


  ## Related

- [`aanban/esp32_beo4`](https://github.com/aanban/esp32_beo4) — Beo4 IR remote library (used here for the confirmed `BEO_CMD_*` command table)
- A sister project, `BeoPowerlinkDisplay`, provided an independent second bus reader used throughout development to confirm signal issues were real and not specific to the Beolab 3500