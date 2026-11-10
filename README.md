# Beolab3500-Standalone

An ESP32 firmware that lets a Bang & Olufsen **Beolab 3500** satellite speaker activate a source without its real Master unit present. Unlike the Menu + 0 + 4 + GO workaround (MK2: Menu + Menu + 0 + 4 + GO), this stays permanent even after a power loss — and on Mk I it also gives more possibilities, like selecting different sources.

## Mk I vs Mk II

The two versions look the same from the outside but work completely differently:

| | Mk I | Mk II (S/N > 19343452) |
|---|---|---|
| Connector | MCL | Powerlink |
| Behaves like | a B&O MCL X-TRA speaker kit | a normal PL speaker (BeoLab 6000/8000), but works only with a Beolink Wireless 1 |
| The ESP32 simulates | a Beomaster | a Beolink Wireless 1 |
| Remote, source select, volume on the BL3500 | yes | no - everything is controlled at the source |
| Audio input | speaker level, amplifier recommended | PL level - attenuate, no amplifier |
| Workaround (not used here) | Menu + 0 + 4 + GO | Menu + Menu + 0 + 4 + GO |

## 2 versions, 3 modes

| Mode | Version | Setup | What the ESP32 does |
|---|---|---|---|
| [Standalone](#beolab-3500-mki) | Mk I | ESP32 + own audio source (e.g. Bluetooth + amplifier), no B&O Master | simulates a Beomaster: answers the source requests from the Beolab's own remote |
| [Standalone](#mk2-standalone) | Mk II | ESP32 + own audio source (attenuated), no B&O Master | simulates a Beolink Wireless 1: sends the init frame at boot |
| [PL speaker](#mk2-pl-speaker) | Mk II | Y-DIN8 cable between a Beomaster/Beovision PL socket and the Beolab, ESP32 tapped in | sends the init frame at boot; audio and volume come from the Master (e.g. as TV center) |

All three modes run on the same firmware (`src/main-standalone.cpp`); Mk I vs Mk II is detected at boot via GPIO43.

The firmware builds for several boards (M5Stack Stamp S3, ESP32-S3-MINI-1, classic ESP32 wrover - pins per board in `src/common/BoardConfig.hpp`); all pin numbers in this README refer to the **M5Stack Stamp S3**.

## Audio source

Several Bluetooth devices have been tested as audio source - the results will follow soon in [Audio.md](Audio.md).

## Switching to PL/MCL mode

Before this project can talk to a Beolab 3500, the speaker itself has to be switched into PL/MCL bus mode via its own menu:

- **Mk II**: Menu, Menu, 0, 9, Go, then Up/Down to (off), Go.
- **Mk I**: Menu, 0, 9, Go, then Up/Down to Off, Go.  (not sure if realy needed)



## Beolab 3500 MKI
Testd on SW 1.1
#### The basic idea

When you press a source key on the Beolab 3500's own remote:

1. **Beolab 3500 → bus**: a 17-bit notify frame naming the source (Radio, here):

   ```
   00000000110000001
   ```
   Format(3)=`000`, Address(to)(5)=`00000`, Address(from)(4)=`1100` (=12, its own bus address), Data(5)=`00001` (=1, Radio's Beo4 key code)

2. **This board → bus**: the reply a real Master would send. Two real Master styles were sniffed, selected in `setup()` through `MclMasterVariant` (currently `MCL`, see `common/MclBusWriter.cpp`):

   - **MCL** (an older Master, e.g. a BM4500): no Sound frame. A VOLUME and a channel/track SelectSource frame (40 bit each), sent as a pair **twice**.
   - **PL** (Beocenter 2300): a Sound frame plus a SelectSource frame, as below.

   Sound (47 bits, PL style):
   ```
   00110011010011101011000000001111000001010001000
   ```
   SelectSource (48 bits, PL style):
   ```
   001110111100000101100000000001000000001000000000
   ```

Without step 2, the Beolab 3500 receives its own notify but never actually activates the source. See [Protocol notes](#protocol-notes-mcl-2-datalink-86) and [How it works](#how-it-works) below for the full field-level breakdown.





#### MCL DIN8 connector pinout

The Beolink MCL cable carries more than just the bus — this project only taps 2 of its 7 pins (Data + Shield/GND, see [bus interface](#schematic-bus-interface) above). The other pins are listed for reference only: the audio side (jack, level, termination, amplifier) differs per Beolab version and source and is intentionally not covered here.

```
Pin 1 ─── Yellow (L hot)        audio, not covered here
Pin 2 ─── Pink   (DC 7.5–8.5V)  not used
Pin 3 ─── Grey   (L gnd)        audio, not covered here
Pin 4 ─── Green  (R hot)        audio, not covered here
Pin 5 ─── Brown  (R gnd)        audio, not covered here
Pin 6 ─── White  (Data)         see Schematic (bus interface)
Pin 7 ─── Shield (GND)          see Schematic (bus interface)
```

MCL expects a speaker-level audio signal. Line level works, but the volume is limited and you hear hiss during pauses with the Beolab turned all the way up, so an amplifier is recommended.

With a real Beomaster: connect the two 3-pin plugs to its Speaker 2 sockets (they carry the MCL data); cable pinout on [BeoTech](https://archived.beoworld.org/beotech/wiring1.htm).

#### Schematic (bus interface)

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
                    GPIO9  (external mute input) ◄──────────────────────────────────  external mute source (LOW = mute)
```

Pin numbers above are for `standalone_m5_stamp_S3` (the active `default_envs`); `standalone_esp32_wrover` uses different physical pins for the same circuit (GPIO34/25/26 for RX/TX/Mute, GPIO33 for the external mute input, GPIO32 for MK2 detection - see the `BOARD_WROOVER` block in `src/common/BoardConfig.hpp`).

R5 (pull-up) is only needed when no Master is on the bus. On a Y-DIN8 cable to a Beomaster/Beovision it isn't necessary - the Master provides the pull-up.

All named pins on `standalone_m5_stamp_S3`, and what each one does in MK1 vs MK2 (`blVersion` is now auto-detected at boot via `MK2_DETECTED`, not hand-set - see `src/main-standalone.cpp`'s `setup()`):

| Pin | MK1 | MK2 |
|---|---|---|
| GPIO1  | `MCL_RX_PIN` - reads the bus | unused |
| GPIO3  | `MCL_TX_PIN` - sends on the bus | `MCL_TX_PIN` - sends on the bus |
| GPIO5  | `KEY_PIN_LEFT` (nav key output*) | `MK2_MUTE_PIN` (mute output to Beolab pin 4) |
| GPIO7  | `KEY_PIN_RIGHT` (nav key output*) | `MK2_DISPLAY_OFF_PIN` (jumper, work in progress) |
| GPIO9  | `KEY_PIN_STOP` (nav key output*) | `MK2_EXT_MUTE_PIN` (reads an external mute signal, LOW = mute) |
| GPIO43 | `SOURCE_PINS` Radio output *(after boot)* | `MK2_DETECTED` (read once at boot to pick MK1 vs MK2) |
| GPIO44 | `SOURCE_PINS` TV output | unused |

\* e.g. to a Bluetooth module with nav keys like the QCC5125.

GPIO5/9/43 are deliberately shared between an MK1-only and an MK2-only purpose - safe because `blVersion` is fixed for the whole run (decided once at boot from GPIO43) and the MK1-only code (`beginKeyPins()`/`pressKey()`/`beginSourcePins()`) vs MK2-only code (the external-mute mirror block in `loop()`, which only writes `MK2_MUTE_PIN` when the external input changes - so a manual `mute 1|0` isn't overwritten) never both run in the same boot. GPIO43 is the odd one out: it's read once as `MK2_DETECTED` *before* `blVersion` is known, then - only on MK1 - repurposed as the Radio source-select output for the rest of that run.


#### Schematic (per-source select outputs)

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

#### Schematic (navigation key outputs) could be used for Bluetooth navigation

Same pattern as the per-source outputs above, but for the Left/Right/Stop keys (`KEY_PINS[]` in `src/common/GpioOutputs.cpp`, dispatched from `GpioOutputs::handleNavKeys()`), intercepted *before* the source-select mapping — Left(18)/Right(20) would otherwise collide with real device numbers once `+192` is applied (18+192=210=CD, 20+192=212=A.Tape2). Idea: drive a Bluetooth controller's Next/Prev/Pause. Not wired up yet, and unlike the sources, these three key values are only derived from the same `&0x1F` formula — not individually confirmed against real Beolab 3500 hardware:

```
ESP32 - standalone_m5_stamp_S3
┌───────────────────────────┐
│  GPIO5  (Left)   ●────────┼──► HIGH while Left is pressed   (-> Bluetooth Prev?)
│  GPIO7  (Right)  ●────────┼──► HIGH while Right is pressed  (-> Bluetooth Next?)
│  GPIO9  (Stop)   ●────────┼──► HIGH while Stop is pressed   (-> Bluetooth Pause?)
└───────────────────────────┘
```

GPIO5/9 are shared with `MK2_MUTE_PIN`/`MK2_EXT_MUTE_PIN` (MK1-only vs MK2-only, see the pin table above) - not a typo.

#### Protocol notes (MCL-2 "Datalink '86")

[Bang_and_Olufsen_MCL-2_service_manual.pdf](Bang_and_Olufsen_MCL-2_service_manual.pdf)



## Beolab 3500 Mk II

Tested with SW 3.33


According to the service manual the Mk II's PL connector is only intended for the Beolink Wireless 1 — the ESP32 takes that role here.

The Mk II runs in one of two modes (see [2 versions, 3 modes](#2-versions-3-modes)) - init, mute and display below apply to both.

#### The basic idea

To make the Mk II play, two things are needed: **pin 4 (Mute) HIGH**, and the init frame `0011000111100111111100000000100` sent once on **pin 6 / 7**, followed by one extra t1 pulse (3.125 ms) after its Stop (t4). The firmware does both at boot (`sendInit()`).

The mute pin (`MK2_MUTE_PIN`, Beolab pin 4) mutes the speaker itself: `mute 1|0` on Serial (1 = LOW = muted), released (HIGH) at the end of `sendInit()`, and it follows an external mute input (`MK2_EXT_MUTE_PIN`) when that changes.

#### Display

The display is indication only and has no effect on the sound. In [MK2 Standalone](#mk2-standalone) it hardly makes sense; in [MK2 PL Speaker](#mk2-pl-speaker) it does - with the display the Beolab 3500 then behaves less like a BeoLab 6000/8000 and more like a BeoLab 3000/4500/5000. It is currently **always on**.

For testing, `<source>`, `vol <value>` and `off` change only the display (source name, a volume number, display off).

**TODO:** switch the display on/off per jumper (`MK2_DISPLAY_OFF_PIN`, GPIO7).

#### Schematic (bus interface)
see MK I

#### PL DIN8 connector pinout
The Powerlink cable carries more than just the bus — this project taps Data, Shield/GND and Mute (see bus interface above). The other pins are listed for reference only: the audio side differs per Beolab version and source and is intentionally not covered here (mind the PL level warning above).

```
Pin 1 ─── Grey   (power)  not used
Pin 2 ─── Blue   (GND)    audio GND, not covered here
Pin 3 ─── Brown  (Left)   audio, not covered here
Pin 4 ─── Yellow (Mute)   see Schematic (bus interface)
Pin 5 ─── Green  (Right)  audio, not covered here
Pin 6 ─── White  (Data)   see Schematic (bus interface)
Pin 7 ─── Shield (GND)    see Schematic (bus interface)
```

### MK2 Standalone

No B&O Master: the ESP32 sits alone on the Beolab's PL socket (with R5 as pull-up) and you feed your own audio source.

> ⚠️ **Big warning**: PL-level audio signals are much lower than a typical Line In / audio jack level. Since the Beolab 3500 Mk II's volume can't be controlled over PL (see above), feeding it a normal Line-In-level signal directly risks destroying it. Attenuate the input first — a voltage divider, or a level control on the source device — before connecting it. Do not use an amplifier.

### MK2 PL Speaker

When the Beolab 3500 Mk II is in PL mode, it behaves like a normal BL speaker (BL6000, BL8000): no IR reception, no SelectSource, and **also no Vol+/Vol-**. A speaker like that having its own display doesn't really make sense in the first place.

The Beolab works as a normal PL speaker of a Beomaster/Beovision (e.g. TV center): a Y-DIN8 cable between the Master's PL socket and the Beolab, the ESP32 tapped in. Audio and volume come from the Master; R5 isn't necessary. The Powerlink socket doesn't supply enough current for the ESP32 — power it from an external 5 V supply.

Open points (TODO):
- Mute: the ESP32 drives the mute pin (pin 4) and overrides the Master's mute.
- The pull-up (R5) isn't necessary on the Y-DIN8 cable - possibly a 4-line / 8-line jumper.

## Project structure

Both Beolab 3500 revisions share one bus implementation (confirmed identical wire protocol) and one entry point:

- `src/main-standalone.cpp` — auto-detects MK1 vs MK2 via GPIO at boot and points a single `BusWriter *writer` at the matching subclass:
  - **MK1**: fully automatic — read an MCL/PL notify frame, filter for the Beolab 3500's request, reply, drive the active source pin.
  - **MK2**: no automatic flow (BL3500 Mk2 doesn't send anything of its own onto the bus — it's a passive speaker, all traffic originates from the real Master) — `setup()` sends a built power-on sequence once at boot (`writer->sendInit()`), and `SerialDebugCommands`'s Serial commands are otherwise the only way to send anything - see [Serial commands](#serial-commands-and-log-output).
- `src/common/`:
  - `BL3500Version.hpp` — the `MK1`/`MK2` enum, decided once at boot and used to pick the writer subclass and gate MK1-/MK2-only code
  - `SerialDebugCommands.*` — the Serial command line, polled from `main-standalone.cpp`'s `loop()`. It only parses commands and dispatches to the writer (plus its own `verbose`/`?`); everything a writer sends or switches is logged by the writer itself
  - `BusReader.*` — RMT-based bus capture and pulse-to-bit decoding, shared by both revisions (MK1 only calls `begin()`/`poll()` on it - MK2 has nothing to read)
  - `BusSniff.hpp` — the listen-only **sniffer**: logs every frame `BusReader` delivers that isn't our own echo (see [Serial commands](#serial-commands-and-log-output))
  - `BusWriter.*` — base class: bit-to-pulse encoding/transmission (`begin`/`sendFrame`/`pulse`, identical for both revisions), echo detection (`consumeEcho`, so our own TX isn't shown as Master traffic) and the `sendSource`/`sendVol`/`sendInit`/`sendOff`/`sendMute` interface, virtual with harmless "not available" defaults (not pure virtual; `sendStandby` is implemented here for every writer), implemented for real by:
    - `MclBusWriter.*` — MK1's real frame content (MCL or PL style, see `MclMasterVariant`)
    - `PlBusWriter.*` — MK2's real frame content (including its extra trailing pulse and the optional mute pin - see [Beolab 3500 Mk II](#beolab-3500-mk-ii))
  - `MclData.*` — frame parsing/building (header fields, device mapping, Sound/SelectSource frame construction) — parsing is confirmed against MK1 only; the builders don't take a `BL3500Version` (each subclass above just calls them with different arguments)
  - `GpioOutputs.*` — the downstream per-source and navigation-key GPIO outputs, shared since they're hardware-side, not protocol-specific (MK1 only for now). `handleNavKeys()` recognizes Left/Right/Stop notify frames and drives the matching `KEY_PINS[]` output instead of a source reply (see [navigation key outputs](#schematic-navigation-key-outputs) below)

## How it works

1. `BusReader` continuously decodes bus traffic and hands complete frames to `loop()`. Our own transmissions come back over the shared wire and are dropped there (`BusWriter::consumeEcho`); every other frame goes through the sniffer (`BusSniff::logFrame`) before anything else looks at it.
2. `MclData` parses each frame's header and, if it matches the Beolab 3500's short notify pattern, resolves which source was requested (`device = data + 192`, cross-checked against the full Beo4 command table — see source comments for how that formula was derived).
3. `GpioOutputs::handleNavKeys()` intercepts Left/Right/Stop and drives a `KEY_PINS[]` output (see [navigation key outputs](#schematic-navigation-key-outputs) above) instead of a source reply.
4. `loop()` calls `writer->sendSource(device, track)` (see `common/MclBusWriter.cpp`), which replies with a SelectSource frame for the requested device, as a real Beocenter 2300 would.
5. One GPIO per audio source is also driven HIGH for whichever source is currently active (`GpioOutputs::setActiveSourcePin()`, called from `main-standalone.cpp` right after `writer->sendSource()`) — meant for a separate relay/routing board to pick up which physical audio input should be live, with no protocol knowledge needed on that side.


## Serial commands and log output

Type `?` (or `help`) in the serial monitor (115200 baud) for this list; the firmware prints a hint at start-up.

| Command | What it does |
|---|---|
| `<source> [track]` | select a source, e.g. `cd 6`, `radio` (tv radio v.aux a.aux v.tape dvd sat pc a.tape cd phono a.tape2 cd2, or a device number 192..215) |
| `off` | switch off - MCL: the whole system; MK2: the display only |
| `standby` (`alloff`, `allstandby`) | Beo4 ALL STANDBY frames (untested reference, does not switch off an MCL setup) |
| `init` / `init <value>` | send the init sequence / an init with a test value |
| `vol <value>` | set the volume (MK2 only, display only) |
| `mute 1` / `mute 0` | mute on (MK2 pin LOW) / off (HIGH) (MK2 only) |
| `verbose` / `v` | toggle the sniffer's verbose mode |
| `?` / `help` | this list |

Every action is logged as a **trigger** followed by the indented **response** of the writer:

```
[cmd] cd 6                        <- typed on Serial
   -> MCL sends: source CD, track 6
[BL] key CD                       <- a key on the Beolab 3500's own remote (MK1)
   -> MCL sends: source CD, track 1
   -> output CD: GPIO43
[cmd] mute 1
   -> output mute: GPIO26 LOW
```

Nothing is logged between the pulses of a frame - logging happens before the first or after the last pulse.

### Sniffer

While the firmware runs on MK1 it also listens: every frame on the bus that isn't our own echo is decoded and printed, so a real Master can be watched next to our own triggers. The gap to the previous frame is shown in ms:

```
[+397ms] [MASTER PL] Sound VOLUME = 18  (47 bit) | hex: 33 4E B0 09 82 [0101000]
[+398ms] [MASTER PL] SelectSource CD: activate seek=0 value=0  (48 bit) | hex: ...
[+120ms] [MASTER MCL] SelectSource Radio: volume seek=108 value=34  (40 bit) | hex: ...
[+50ms]  [MASTER ?] unknown cmd 49  (31 bit) | hex: ...
```

- The Master style (`PL` / `MCL`) is inferred from the frame's ValueType (72/64 = MCL-style volume and channel/track pair, 96 = PL-style activate, a Sound frame = PL) - not confirmed by a second source.
- Short, garbled and empty captures (e.g. the 1-bit `0` a Master's trailing pulse leaves behind) are hidden by default; `verbose` shows them as `[RX short]` with bits and hex.
- MK2 has no receive path, so the sniffer only exists on MK1.
- For deeper captures (raw RMT timing) the sister project `BeoPowerlinkDisplay` has a fuller sniffer.

## License

[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — free to use, modify, and share (with attribution, same license for derivatives), non-commercial only. See [`LICENSE`](LICENSE).


  ## Related

- A sister project, `BeoPowerlinkDisplay`, provided an independent second bus reader used throughout development to confirm signal issues were real and not specific to the Beolab 3500