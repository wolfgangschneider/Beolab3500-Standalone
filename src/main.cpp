/*
  Beolab3500-Standalone - MCL/PL bus Master emulator for Beolab 3500
  startup

  Handles both Beolab 3500 hardware revisions from one firmware -
  set `blVersion` below to match the board you're about to flash, then
  build+upload. MK1 and MK2 share the exact same wire-level protocol
  (t1..t5 timing, AGC preamble, differential bit encoding - see
  common/MclBusReader.hpp) but differ in frame *content*:
  - MK1: BL3500 sends a short "notify" frame whenever a source key is
    pressed on its own remote; we reply with the same Sound +
    SelectSource frames the real Master (a Beocenter 2300) sends -
    without that reply BL3500 never activates the requested source.
    Fully automatic, driven by loop()'s notify-decode path below.
  - MK2: whose real Master is a separate "Beolink Wireless BL" unit.
    BL3500 Mk2 doesn't send anything of its own onto the bus - it's a
    passive speaker (like a BL6000/BL8000, no IR reception, no
    SelectSource), all traffic originates from the real Master. So
    there's no notify to react to and no automatic reactive flow;
    activation instead works by
    replaying/building known-good frame sequences, either once at boot
    (see setup()) or via the debug Serial commands (see
    handleDebugSerial()). Real MK2 Master traffic also has one extra
    trailing low strobe pulse after Stop, which `writer` reproduces
    when blVersion=MK2 (see common/MclBusWriter.hpp).

  Board: ESP32 WROVER DevKit (upesy_wrover). RX on GPIO34 (via R3/R4
  divider, see MclBusReader), TX on GPIO25 (via transistor Q1 switch) -
  same electrical interface for both revisions.

  Per B&O MCL-2 Service Manual ("Datalink '86"), MK1 frame content:
  - Timing symbols: t1=3.125ms t2=6.250ms t3=9.375ms (data),
    t4=12.500ms (Stop), t5=15.625ms (Start). Start is preceded by two
    AGC-priming t1 pulses (manual fig. 2045-4: "1 1 5") - a real
    receiver's analog front-end needs these to lock on, even though a
    pure digital RX decodes fine without them.
  - BL3500's notify is a short frame (data < 8 bits): addrFrom=12 (its
    own bus address), addrTo=0, data = BEO_CMD_XXX & 0x1F (TV=0,
    Radio=1, CD=18 confirmed).
*/

#include <Arduino.h>
#include "common/MclBusReader.hpp"
#include "common/MclBusWriter.hpp"
#include "common/MclData.hpp"
#include "common/GpioOutputs.hpp"

// set this to match the board you're about to flash - see file header
static BL3500Version blVersion = BL3500Version::MK2;

constexpr gpio_num_t MCL_RX_PIN = GPIO_NUM_34;
constexpr gpio_num_t MCL_TX_PIN = GPIO_NUM_25;

// MK2 only: BL3500 Mk2's display doesn't update correctly unless this
// is driven around writer.sendInit() - own dedicated pin, deliberately
// not GPIO14 (that's GpioOutputs::KEY_PIN_STOP, MK1's Stop nav key -
// must stay distinct even though MK1/MK2 never run in the same build).
// NOT GPIO34/35/36/39 - those are input-only on the ESP32 (no output
// driver), confirmed the hard way: pinMode(35, OUTPUT) failed
// ("GPIO can only be used as input mode"). GPIO26 has an output
// driver and isn't a boot-strapping pin (unlike 0/2/12/15) - change if
// it's not physically reachable on the board either.
constexpr gpio_num_t MK2_MUTE_PIN = GPIO_NUM_26;

// BACKUP of Master's Sound response as a raw bitstring, kept in case
// MclData::buildSoundBits() ever needs to be double-checked against
// the original capture. Not used anymore - see MclBusWriter::sendSound()
// for the dynamic equivalent: MclData::buildSoundBits(78, subType, value).
// Bytes: 33 4E B0 0F 05 -> Command=51 (Sound), Type=78, SubType=3
// (bit-shifted field, see BuOPowerlink/PowerLink.cpp), Value=68.
constexpr const char *MASTER_RADIO_SOUND_BITS = "00110011010011101011000000001111000001010001000";

static MclBusWriter writer(MCL_TX_PIN, blVersion);
static MclBusReader reader(MCL_RX_PIN);

// Sender address navigation-key notify frames were observed to use,
// distinct from BL3500_ADDR (12) used for source-select notifies. Not
// otherwise identified (same open question as the still-unexplained
// address 11 seen elsewhere) - only used here to separate navigation
// keys from source keys that alias to the same Beo4 value (see
// handleBl3500Key() call site in loop() for the specific collisions).
// MK1 only.
constexpr uint32_t NAV_KEY_ADDR = 9;



// Called for every BL3500 notify with the raw Beo4 key code it
// reported (before it's mapped to a source device via +192 - Left/
// Right/Stop would otherwise collide with real source device numbers:
// 18+192=210=CD, 20+192=212=A.Tape2). Returns true if the key was
// handled here, so loop() skips the normal source-select reply for it.
// Key values are (BEO_CMD_XXX & 0x1F) from the esp32_beo4 library,
// same formula as the sources - not yet verified against real
// hardware for these three. MK1 only.
static bool handleBl3500Key(uint32_t key) {
  gpio_num_t pin;
  switch (key) {
    case 18: pin = GpioOutputs::KEY_PIN_LEFT;  break; // Left  (BEO_CMD_LEFT  0x32 & 0x1F)
    case 20: pin = GpioOutputs::KEY_PIN_RIGHT; break; // Right (BEO_CMD_RIGHT 0x34 & 0x1F)
    case 22: pin = GpioOutputs::KEY_PIN_STOP;  break; // Stop  (BEO_CMD_STOP  0x36 & 0x1F)
    default: return false;
  }
  Serial.printf("-> key %u: GPIO%d pressed\n", key, (int) pin);
  GpioOutputs::pressKey(pin);
  return true;
}

// sendSource/sendSound/sendVol/sendInit now live on MclBusWriter (see
// common/MclBusWriter.hpp) - moved there so they're reusable and
// self-contained (they already knew `_version`, no need to pass
// blVersion around). GpioOutputs::setActiveSourcePin() stays here in
// main.cpp on purpose - it's a downstream hardware-output concern the
// bus writer shouldn't need to know about; call sites below call it
// explicitly right after writer.sendSource() for MK1.

// DEBUG: type a line over Serial to test without the real Master
// present. Not part of MK1's normal notify-driven flow; MK2 has no
// automatic flow at all yet, so this is its only way to send anything
// besides the boot sequence in setup(). Same commands for both
// revisions except "init"/"vol" (MK2 only, no MK1 equivalent):
//   "sound <subType> <value>" (e.g. "sound 3 68") - calls
//     writer.sendSound(subType, value), to find out what those
//     actually do on real hardware.
//   "<source name>" (e.g. "radio", "cd", "tv", ...) or a bare device
//     number (e.g. "193"), optionally followed by a track value (e.g.
//     "radio 4", default 0) - calls writer.sendSource(device, track),
//     as if BL3500 had just requested it (writer.sendSource() branches
//     on its own _version internally, so this is the same call either way).
//   "init"   - MK2 only: calls writer.sendInit() - see common/MclBusWriter.cpp for what it currently sends
//   "vol <value>" - MK2 only: calls writer.sendVol(value)
static void handleDebugSerial() {
  static String buf; // accumulates across loop() calls, so slow typing never times out

  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      buf += c;
      continue;
    }
    if (buf.length() == 0) continue; // ignore a bare \r\n pair / empty line

    String line = buf;
    buf = "";
    line.trim();

    if (blVersion == BL3500Version::MK2) {
      String lower = line;
      lower.toLowerCase();

      if (lower == "init") {
        digitalWrite(MK2_MUTE_PIN, LOW); // ensure mute is on while the init sequence runs
        writer.sendInit();
        digitalWrite(MK2_MUTE_PIN, HIGH); // ensure mute is off after the init sequence
        continue;
      }

      int volValue;
      if (sscanf(line.c_str(), "vol %d", &volValue) == 1) {
        Serial.printf("-> debug Vol frame: value=%d\n", volValue);
        writer.sendVol((uint8_t) volValue);
        continue;
      }

      // "radio"/"radio <track>" is NOT handled here - it falls through
      // to the shared source-name+track dispatch below (same as MK1),
      // calling writer.sendSource(193, track). The literal 6-frame real
      // Radio-press capture this used to replay (SelectSource,Sound,
      // SelectSource,Sound,SelectSource,Sound - gap2="10101010" on its
      // Sound frames, not reproducible via buildSoundBits(), see git
      // history) is no longer wired to any command.

      // none of the fixed MK2 commands matched (e.g. "init" typed
      // wrong, or "radio" with no args) - fall through to the shared
      // source-name+track dispatch below, so "radio 5" etc. also works
      // here, not just for MK1 (writer.sendSource() already branches
      // on its own _version).
    }

    int subType, value;
    if (sscanf(line.c_str(), "sound %d %d", &subType, &value) == 2) {
      Serial.printf("-> debug Sound frame: subType=%d value=%d\n", subType, value);
      writer.sendSound((uint8_t) subType, (uint8_t) value);
      continue;
    }

    // "<source name>" or "<source name> <track>" (e.g. "radio 4") -
    // track defaults to 0 if not given.
    String nameToken = line;
    int track = 0;
    int spaceIdx = line.indexOf(' ');
    if (spaceIdx >= 0) {
      nameToken = line.substring(0, spaceIdx);
      track = line.substring(spaceIdx + 1).toInt();
    }
    int device = MclData::deviceFromName(nameToken);
    if (device < 0 && nameToken.toInt() >= 192) device = nameToken.toInt();
    if (device >= 0) {
      writer.sendSource((uint8_t) device, (uint8_t) track);
      if (blVersion == BL3500Version::MK1) GpioOutputs::setActiveSourcePin((uint8_t) device);
      continue;
    }

    Serial.println("debug: expected \"sound <subType> <value>\" (e.g. \"sound 3 68\") or a source name (radio, tv, cd, ...), optionally followed by a track value (e.g. \"radio 4\")");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  if (blVersion == BL3500Version::MK1) {
    GpioOutputs::beginKeyPins();
    Serial.println("Beolab3500-Standalone MK1 - MCL/PL Master emulator");
    writer.begin();
    reader.begin();
    GpioOutputs::beginSourcePins();
    return;
  }

  // MK2
  Serial.println("Beolab3500-Standalone MK2 - Master emulator");
  writer.begin();
  // reader.begin() intentionally not called: BL3500 Mk2 doesn't send
  // anything of its own to react to (see file header) - loop() has no
  // reactive decode path for MK2 (see loop() below), so nothing would
  // ever drain the RX queue.

  // Important: toggling mute around the init sequence is only
  // necessary for the display - otherwise the source still activates,
  // but the display doesn't update correctly (it only updates once
  // the init sequence has completed). See MK2_MUTE_PIN above.
  pinMode(MK2_MUTE_PIN, OUTPUT);

  // NOTE: unlike the "init" debug command below, mute is NOT pulled
  // LOW before this boot-time sendInit() call - only set HIGH after.
  // Not yet confirmed whether that's deliberate or a leftover gap.
  writer.sendInit();
  digitalWrite(MK2_MUTE_PIN, HIGH); // ensure mute is off initially
}

void loop() {
  handleDebugSerial();

  if (blVersion == BL3500Version::MK2) return; // no automatic flow yet - handleDebugSerial()'s commands are the only TX trigger

  // 1. wait (briefly - so the Serial check above stays responsive) for
  //    the next complete frame; see MclBusReader::poll
  String bits;
  if (!reader.poll(bits, pdMS_TO_TICKS(50))) return;

  // 2. our own TX also reflects back onto RX via the shared bus wire,
  //    but it's always a long Sound/SelectSource frame - step 4 below
  //    (data<8 bits) already filters those out by content, so it's not
  //    separately suppressed by frame count here anymore. Risk,
  //    accepted: if an echo ever decodes with an error/glitch (real
  //    incident, see git history) into something that *looks* short,
  //    it could slip through as if it were a real notify.
  //Serial.printf("frame: %u bits  %s\n", bits.length(), bits.c_str());

  // 3. decode Format+AddrFrom+Data; bail if too short to have a header
  MclData frame(bits);
  if (!frame.valid) return;
 // Serial.printf("  addrFrom=%u data(%u bit)=%u\n",
 //               frame.addrFrom, frame.data.length(), MclData::bitsToValue(frame.data));

  // 4. only react to short notify-shaped frames (data<8 bits); ignore
  //    Master traffic and other long frames. addrTo isn't checked - every
  //    real notify had the same constant value there, so it never
  //    discriminated anything.
  if (frame.data.length() >= 8) return;

  // 5. Left/Right/Stop switch a GPIO instead of a source reply. Gated on
  //    addrFrom==NAV_KEY_ADDR (not just key value) because the Beo4 key
  //    space is shared between navigation and source commands: CD's key
  //    (0x92&0x1F=18) equals Left's (0x32&0x1F=18), and A.Tape2's
  //    (0x94&0x1F=20) equals Right's (0x34&0x1F=20). Without the address
  //    check, a real CD/A.Tape2 request from BL3500 (addrFrom=12) would
  //    misfire the Left/Right pin instead of (or as well as) replying -
  //    navigation keys were observed coming from a *different* sender
  //    address (9) than BL3500's own source notify (12), so checking
  //    addrFrom here is what actually disambiguates the two.
  uint32_t key = MclData::bitsToValue(frame.data);
  if (frame.addrFrom == NAV_KEY_ADDR && handleBl3500Key(key)) return;

  // 6. everything else must be BL3500's own source-select notify
  //    (addrFrom=12) with a Beo4 key code (& 0x1F) already mapped to a
  //    BODev_* device by the MclData constructor; ignore anything else
  if (frame.addrFrom != MclData::BL3500_ADDR || frame.device < 0) return;

  // 7. reply as Master would, and drive the matching source pin
  writer.sendSource((uint8_t) frame.device, 1);
  GpioOutputs::setActiveSourcePin((uint8_t) frame.device);
}
