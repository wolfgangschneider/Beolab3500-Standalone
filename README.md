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
                                           │                              │
                                          [R3]                            │ (collector)
                                          10k                           ┌─┴─┐
                                           │                            │   │
                                GPIO34 ────┤                            │Q1 │  BC847 (NPN)
                                (RX)       │                  (base)    │   │
                                         [R4]              ┌────────────┤   │
                                          15k              │            └─┬─┘
                                           │              [R1]            │ (emitter)
                                          GND             2k2             │
                                                           │             GND
                                GPIO25 ────────────────────┤
                                (TX)                       │
                                                          [R2]
                                                          10k
                                                           │
                                                          GND

                    ESP32 — GND   ───────────────────────────────────────────► Beolab 3500 (pin 7)
 
 
 Beolab MK2 
                    ESP32 - 34  you don't need the  GPIO34 ─ line above
                    ESP32 - 5v+    ───────────────────────────────────────────► Beolab 3500 MKII (pin 4)
                    or (Optional MKII with display)
                    ESP32 Pin ?    ───────────────────────────────────────────► Beolab 3500 MKII (pin 4)
```

### Schematic (per-source select outputs)

One GPIO per audio source (`SOURCE_PINS[]` in `src/common/GpioOutputs.cpp`), driven HIGH for whichever source is currently active and LOW for all others. A separate board reads these directly — no decoding needed on its side:

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

Pin numbers are placeholders for the current dev board (`upesy_wrover`) and free to reassign .

### Schematic (navigation key outputs) could be used for Bluetooth navigation

Same pattern as the per-source outputs above, but for the Left/Right/Stop keys (`KEY_PINS[]` in `src/common/GpioOutputs.cpp`, dispatched from `handleBl3500Key()` in `src/main.cpp`), intercepted *before* the source-select mapping — Left(18)/Right(20) would otherwise collide with real device numbers once `+192` is applied (18+192=210=CD, 20+192=212=A.Tape2). Idea: drive a Bluetooth controller's Next/Prev/Pause. Not wired up yet, and unlike the sources, these three key values are only derived from the same `&0x1F` formula — not individually confirmed against real Beolab 3500 hardware:

```
ESP32 - wrover⚠️ work in progress, will change
┌───────────────────────────┐
│  GPIO12 (Left)   ●────────┼──► HIGH while Left is pressed   (-> Bluetooth Prev?)
│  GPIO13 (Right)  ●────────┼──► HIGH while Right is pressed  (-> Bluetooth Next?)
│  GPIO14 (Stop)   ●────────┼──► HIGH while Stop is pressed   (-> Bluetooth Pause?)
└───────────────────────────┘
```

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
1. After the frame's normal Stop (t4), one more t1 pulse (3.125ms).
2. Mute (`MK2_MUTE_PIN`, GPIO26 in `main.cpp`) must only go HIGH *after* that init sequence (`0011000111100111111100000000100`) has finished sending — it has to stay LOW for the whole duration of the sequence.

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

Both Beolab 3500 revisions share one bus implementation (confirmed identical wire protocol) and one entry point:

- `src/main.cpp` — reads `blVersion` to pick MK1 or MK2 behavior:
  - **MK1**: fully automatic — read an MCL/PL notify frame, filter for the Beolab 3500's request, reply, drive the active source pin.
  - **MK2**: no automatic flow (BL3500 Mk2 doesn't send anything of its own onto the bus — it's a passive speaker, all traffic originates from the real Master) — `setup()` sends a built power-on sequence once at boot (`writer.sendInit()`), and `handleDebugSerial()`'s commands are otherwise the only way to send anything: `init` and `vol <value>` (MK2 only), plus the shared `sound <subType> <value>` and `<source name> [track]` commands also used by MK1.
- `src/common/`:
  - `BL3500Version.hpp` — the `MK1`/`MK2` enum used throughout instead of an unnamed bool flag
  - `MclBusReader.*` — RMT-based bus capture and pulse-to-bit decoding, shared by both revisions
  - `MclBusWriter.*` — bit-to-pulse encoding and transmission, plus the higher-level protocol sends (`sendSource`/`sendSound`/`sendVol`/`sendInit`) built on top of it; shared by both revisions, branches internally on its own `BL3500Version` (e.g. MK2's trailing pulse) — see [Beolab 3500 Mk II](#beolab-3500-mk-ii)
  - `MclData.*` — frame parsing/building (header fields, device mapping, Sound/SelectSource frame construction) — parsing is confirmed against MK1 only; the builders take a `BL3500Version` too and are also used experimentally from MK2
  - `GpioOutputs.*` — the downstream per-source and navigation-key GPIO outputs, shared since they're hardware-side, not protocol-specific (MK1 only for now)

## How it works

1. `MclBusReader` continuously decodes bus traffic and hands complete frames to `loop()`.
2. `MclData` parses each frame's header and, if it matches the Beolab 3500's short notify pattern, resolves which source was requested (`device = data + 192`, cross-checked against the full Beo4 command table — see source comments for how that formula was derived).
3. Left/Right/Stop are intercepted here and just drive a `KEY_PINS[]` output (see [navigation key outputs](#schematic-navigation-key-outputs) above) instead of a source reply.
4. `loop()` calls `writer.sendSource(device, track)` (see `common/MclBusWriter.cpp`), which replies with four frames, exactly as captured off a real Beocenter 2300: Sound, Sound, SelectSource, SelectSource for the requested device.
5. One GPIO per audio source is also driven HIGH for whichever source is currently active (`GpioOutputs::setActiveSourcePin()`, called from `main.cpp` right after `writer.sendSource()`) — meant for a separate relay/routing board to pick up which physical audio input should be live, with no protocol knowledge needed on that side.


## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — free to use, modify, and share (with attribution, same license for derivatives), non-commercial only. See [`LICENSE`](LICENSE).


  ## Related

- [`aanban/esp32_beo4`](https://github.com/aanban/esp32_beo4) — Beo4 IR remote library (used here for the confirmed `BEO_CMD_*` command table)
- A sister project, `BeoPowerlinkDisplay`, provided an independent second bus reader used throughout development to confirm signal issues were real and not specific to the Beolab 3500