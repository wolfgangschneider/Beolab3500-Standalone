/*
  Beolab3500-Standalone - MCL/PL bus Master emulator for Beolab 3500
  startup

  Handles both Beolab 3500 hardware revisions from one firmware -
  `blVersion` below is just the startup default, auto-detected and
  overridden in setup() via the MK2_DETECTED_PIN GPIO, so no source edit
  is needed to switch revision. Which *board* to build for is picked via
  a -D BOARD_* flag in the platformio.ini env - all board-specific pins
  and values live in common/BoardConfig.hpp. MK1 and MK2 share the
  exact same wire-level protocol
  (t1..t5 timing, AGC preamble, differential bit encoding - see
  common/BusReader.hpp) but differ in frame *content*:
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
    common/SerialDebugCommands.cpp). The captured Mk2 power-on frame also needs
    one extra trailing low strobe pulse after Stop, which
    PlBusWriter::sendInit() sends - confirmed init-specific, not a
    general MK2-traffic property (see common/PlBusWriter.cpp).

  Board: one -D BOARD_* flag per platformio.ini env (BOARD_WROOVER,
  BOARD_M5STAMP_S3, BOARD_S3_MINI). RX/TX/mute pins per board are in
  common/BoardConfig.hpp; see BusReader for the RX-side electrical
  interface (same divider+transistor circuit for both revisions, just
  different GPIOs per board).

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
#include "common/BL3500Version.hpp"
#include "common/BusReader.hpp"
#include "common/BusWriter.hpp"
#include "common/MclBusWriter.hpp"
#include "common/PlBusWriter.hpp"
#include "common/MclData.hpp"
#include "common/GpioOutputs.hpp"
#include "common/SerialDebugCommands.hpp"
#include "common/BoardConfig.hpp"
#include "common/BusSniff.hpp"

// TEMP QCC-AT - AT-command bridge to a QCC5124 over UART1. Compiled out
// automatically if src/common/QccAtBridge.hpp is deleted.
// DISABLED by default - build with -D QCC_AT_ENABLE to turn it back on.
#if defined(QCC_AT_ENABLE) && __has_include("common/QccAtBridge.hpp")
#include "common/QccAtBridge.hpp"
#define QCC_AT_BRIDGE_PRESENT 1
#endif

// MK1 vs MK2 is auto-detected at runtime (MK2_DETECTED_PIN, in setup()).
// All board-specific pins live in common/BoardConfig.hpp.
static BL3500Version blVersion = BL3500Version::MK1;

// MK2_MUTE_PIN (BoardConfig.hpp) is MK2-only: driven around
// writer->sendInit() or the Mk2 display won't refresh. Must stay
// distinct from the KEY_PIN_* nav keys and off any input-only pin
// (ESP32 GPIO34-39) - see BoardConfig.hpp.

// only one concrete writer ever exists - constructed with `new` in
// setup() once blVersion is finalized there (MK2 auto-detect via GPIO
// happens there, so it isn't known yet at this point during static
// initialization).
static BusWriter *writer = nullptr;
static BusReader reader(MCL_RX_PIN);
// bound by reference to `writer`/`blVersion` above - see SerialDebugCommands.hpp
static SerialDebugCommands debugCommands(writer, blVersion, MK2_MUTE_PIN);

// sendSource()/sendVol()/sendInit() live on BusWriter's two subclasses
// (see common/MclBusWriter.hpp, common/PlBusWriter.hpp) so they're
// reusable through the one `writer` pointer. GpioOutputs::
// setActiveSourcePin() stays out of BusWriter on purpose - it's a
// downstream hardware-output concern the bus writer shouldn't need to
// know about; call sites call it explicitly right after
// writer->sendSource() for MK1 (see loop() below and SerialDebugCommands.cpp).

void setup() {
  Serial.begin(115200);
  delay(500);

#ifdef QCC_AT_BRIDGE_PRESENT
  QccAtBridge::begin();  // TEMP QCC-AT
#endif

    pinMode(MK2_DETECTED_PIN, INPUT_PULLDOWN);

  if (digitalRead(MK2_DETECTED_PIN) == HIGH) {
    Serial.printf("MK2 detected via GPIO%d\n", (int) MK2_DETECTED_PIN);
    blVersion = BL3500Version::MK2;
  }

  // blVersion is now final - MK1/MK2 setup differs from here on,
  // including which concrete writer type gets constructed below (see
  // writer's declaration above for why this can't happen earlier).
  if (blVersion == BL3500Version::MK1) {
    GpioOutputs::beginKeyPins();
    // MclMasterVariant::PL = Beocenter 2300 style, ::MCL = the other,
    // older real Master style - see MclBusWriter.hpp/.cpp for what each
    // sends. Switch here to A/B test against whichever real Master is
    // actually on the bus.
    MclMasterVariant mclVariant = MclMasterVariant::MCL;
    writer = new MclBusWriter(MCL_TX_PIN,mclVariant); // never deleted - lives for the rest of the run
    Serial.println();
    Serial.printf("Beolab3500-Standalone MK1 (%s)- Master emulator %s\n", MclMasterVariant_NAMES[(int)mclVariant],BOARD_NAME);
    Serial.println("input ? to see all commands");
    writer->begin();
    reader.begin();
     GpioOutputs::beginSourcePins();

    return;
  }

  // MK2
  Serial.printf("Beolab3500-Standalone MK2 (PL)- BW emulator %s\n", BOARD_NAME);
  Serial.println("input ? to see all commands");
  writer = new PlBusWriter(MCL_TX_PIN,MK2_MUTE_PIN); // never deleted - lives for the rest of the run
  writer->begin();
  // reader.begin() intentionally not called: MK2 has nothing to react
  // to and no decode path in loop() to drain it (see file header).

  pinMode(MK2_MUTE_PIN, OUTPUT);
  pinMode(MK2_EXT_MUTE_PIN, INPUT_PULLDOWN);

  // the writer holds MK2_MUTE_PIN LOW for the whole duration of sendInit()
  // (needed for the display to update) and releases it HIGH afterwards;
  // loop()'s mute-mirror takes over from there.
  //Serial.println("[start] init");
  writer->sendInit();
}

void loop() {



  debugCommands.poll();

#ifdef QCC_AT_BRIDGE_PRESENT
  QccAtBridge::poll();  // TEMP QCC-AT - pump QCC5124 replies to USB serial
#endif

  if (blVersion == BL3500Version::MK2) {
    // external mute source (HIGH = mute) -> MK2's own mute pin (Beolab
    // pin 4) - WIP, source not pinned down yet. MK2-gated on purpose: MK2_MUTE_PIN/
    // MK2_EXT_MUTE_PIN share physical GPIOs with MK1-only KEY_PIN_LEFT/
    // STOP (see GpioOutputs.hpp) - safe since MK1 and MK2 code never
    // run in the same boot.
    // Mirrored only when the external mute signal CHANGES (not every tick), so a value
    // set by the "mute 1|0" command isn't overwritten a millisecond later.
    static bool lastMk2Mute = false; // init leaves the pin HIGH = not muted
    bool mk2Mute = digitalRead(MK2_EXT_MUTE_PIN) == HIGH;
    if (mk2Mute != lastMk2Mute) {
      lastMk2Mute = mk2Mute;
      digitalWrite(MK2_MUTE_PIN, !mk2Mute); // mirror the external mute signal to the MK2's own mute pin
    }
    //Serial.printf("loop() tick: MK2_EXT_MUTE_PIN=%d -> MK2_MUTE_PIN=%d\n", mk2Mute, !mk2Mute);
    return; // no automatic flow yet - debugCommands.poll() is the only TX trigger
  }

  // 1. wait (briefly - so the Serial check above stays responsive) for
  //    the next complete frame; see BusReader::poll
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
  if (writer->consumeEcho(bits)) return; // our own TX reflected back
  BusSniff::logFrame(bits);               // every frame, listen-only

  MclData frame(bits);
  if (!frame.valid) return;
  //Serial.printf("  addrFrom=%u data(%u bit)=%u\n",
  //              frame.addrFrom, frame.data.length(), MclData::bitsToValue(frame.data));

  // 4. only react to short notify-shaped frames (data<8 bits); ignore
  //    Master traffic and other long frames. addrTo isn't checked - every
  //    real notify had the same constant value there, so it never
  //    discriminated anything.
  if (frame.data.length() >= 8) return;

  // 5. Left/Right/Stop switch a GPIO instead of a source reply - see
  //    GpioOutputs::handleNavKeys() for why this needs the addrFrom
  //    check too (not just the key value).
  if (GpioOutputs::handleNavKeys(frame)) return;

  

  // 6. everything else must be BL3500's own source-select notify
  //    (addrFrom=12) with a Beo4 key code (& 0x1F) already mapped to a
  //    BODev_* device by the MclData constructor; ignore anything else
  if (frame.addrFrom != MclData::BL3500_ADDR || frame.device < 0) {
    Serial.printf("[BL] ignored: addr=%u device=%d\n", (unsigned) frame.addrFrom, (int) frame.device);
    return;
  }
  Serial.printf("[BL] key %s\n", MclData::deviceName((uint8_t) frame.device)); // trigger: BL3500 source key

  // 7. reply as Master would, and drive the matching source pin
  writer->sendSource((uint8_t) frame.device, 1);
  GpioOutputs::setActiveSourcePin((uint8_t) frame.device);
}
