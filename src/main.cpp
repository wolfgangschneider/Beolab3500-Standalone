/*
  Beolab3500-Standalone - MCL/PL bus Master emulator for Beolab 3500
  startup

  Handles both Beolab 3500 hardware revisions from one firmware -
  `blVersion` below is just the startup default, auto-detected and
  overridden in setup() via the MK2_DETECTED GPIO, so no source edit
  is needed to switch revision. Which *board* to build for (pin
  numbers only) is picked via the platformio.ini env instead - see
  the `#ifdef BOARD_M5STAMP_S3` block below. MK1 and MK2 share the
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

  Board: two platformio.ini envs, `esp32_wrover` (ESP32 WROVER DevKit)
  and `m5_stamp_S3` (M5Stack Stamp S3, the default). RX/TX/mute pins
  differ between them - see the #ifdef block below, or BusReader for
  the RX-side electrical interface (same divider+transistor circuit
  for both revisions, just different GPIOs per board).

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

// set this to match the board you're about to flash - see file header
static BL3500Version blVersion = BL3500Version::MK1;
#ifdef BOARD_M5STAMP_S3
constexpr gpio_num_t MCL_RX_PIN = GPIO_NUM_1;

constexpr gpio_num_t MCL_TX_PIN = GPIO_NUM_3;
constexpr gpio_num_t MK2_MUTE_PIN = GPIO_NUM_5;
constexpr gpio_num_t MK2_BL_MUTE_PIN = GPIO_NUM_9;
constexpr gpio_num_t MK2_DETECTED = GPIO_NUM_43;

String board= "stamp";
#else
constexpr gpio_num_t MK2_MUTE_PIN = GPIO_NUM_26;
constexpr gpio_num_t MK2_BL_MUTE_PIN = GPIO_NUM_33;
constexpr gpio_num_t MK2_DETECTED = GPIO_NUM_32;
constexpr gpio_num_t MCL_RX_PIN = GPIO_NUM_34;
constexpr gpio_num_t MCL_TX_PIN = GPIO_NUM_25;
String board= "wroover";
#endif

// MK2 only: BL3500 Mk2's display doesn't update correctly unless this
// is driven around writer->sendInit() - own dedicated pin, deliberately
// not GPIO14 (that's GpioOutputs::KEY_PIN_STOP, MK1's Stop nav key -
// must stay distinct even though MK1/MK2 never run in the same build).
// NOT GPIO34/35/36/39 - those are input-only on the ESP32 (no output
// driver), confirmed the hard way: pinMode(35, OUTPUT) failed
// ("GPIO can only be used as input mode"). GPIO26 has an output
// driver and isn't a boot-strapping pin (unlike 0/2/12/15) - change if
// it's not physically reachable on the board either.

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
    pinMode(MK2_DETECTED, INPUT_PULLDOWN);

  if (digitalRead(MK2_DETECTED) == HIGH) {
    Serial.printf("MK2 detected via GPIO%d\n", (int) MK2_DETECTED);
    blVersion = BL3500Version::MK2;
  }

  // blVersion is now final - MK1/MK2 setup differs from here on,
  // including which concrete writer type gets constructed below (see
  // writer's declaration above for why this can't happen earlier).
  if (blVersion == BL3500Version::MK1) {
    GpioOutputs::beginKeyPins();
    Serial.printf("Beolab3500-Standalone MK1 - MCL/PL Master emulator %s\n", board.c_str());
    writer = new MclBusWriter(MCL_TX_PIN); // never deleted - lives for the rest of the run
    writer->begin();
    reader.begin();
     GpioOutputs::beginSourcePins();

    return;
  }

  // MK2
  Serial.printf("Beolab3500-Standalone MK2 - Master emulator %s\n", board.c_str());
  writer = new PlBusWriter(MCL_TX_PIN); // never deleted - lives for the rest of the run
  writer->begin();
  // reader.begin() intentionally not called: MK2 has nothing to react
  // to and no decode path in loop() to drain it (see file header).

  // Mute LOW during init is only needed for the display to update
  // correctly - not done here (mute just goes HIGH once, before
  // sendInit()); loop()'s mute-mirror takes over afterwards anyway.
  pinMode(MK2_MUTE_PIN, OUTPUT);

  pinMode(MK2_BL_MUTE_PIN, INPUT_PULLDOWN);

  digitalWrite(MK2_MUTE_PIN, HIGH);
  writer->sendInit();
}

void loop() {
 
 

  debugCommands.poll();

  if (blVersion == BL3500Version::MK2) {
    // mute from BL device - WIP. MK2-gated on purpose: MK2_MUTE_PIN/
    // MK2_BL_MUTE_PIN share physical GPIOs with MK1-only KEY_PIN_LEFT/
    // STOP (see GpioOutputs.hpp) - safe since MK1 and MK2 code never
    // run in the same boot.
    bool mk2Mute = digitalRead(MK2_BL_MUTE_PIN) == HIGH;
    digitalWrite(MK2_MUTE_PIN, !mk2Mute); // mirror the external (BL) mute signal to the MK2's own mute pin
   // Serial.printf("loop() tick: MK2_BL_MUTE_PIN=%d -> MK2_MUTE_PIN=%d\n", mk2Mute, !mk2Mute);
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
  if (frame.addrFrom != MclData::BL3500_ADDR || frame.device < 0) return;

  // 7. reply as Master would, and drive the matching source pin
  writer->sendSource((uint8_t) frame.device, 1);
  GpioOutputs::setActiveSourcePin((uint8_t) frame.device);
}
