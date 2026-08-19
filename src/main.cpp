/*
  Beolab3500-Standalone - MCL/PL bus Master emulator for Beolab 3500 startup

  IMPORTANT: only verified against the Beolab 3500 Mk1. Other Mk
  revisions may differ in bus electrical characteristics or protocol
  details - do not assume this works unmodified on anything else.

  Board: ESP32 WROVER DevKit (upesy_wrover). RX on GPIO34 (via R3/R4
  divider, see PLBusReader), TX on GPIO25 (via transistor Q1 switch).

  Listens for BL3500's short "notify" frame (it sends this whenever a
  source key is pressed on its own remote) and replies with the same
  Sound + SelectSource frames the real Master sends - without this
  reply BL3500 never activates the requested source.

  Per B&O MCL-2 Service Manual ("Datalink '86"):
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
#include "PLBusReader.hpp"
#include "PLBusWriter.hpp"
#include "PLData.hpp"

constexpr gpio_num_t MCL_RX_PIN = GPIO_NUM_34;
constexpr gpio_num_t MCL_TX_PIN = GPIO_NUM_25;

// BACKUP of Master's Sound response as a raw bitstring, kept in case
// PLData::buildSoundBits() ever needs to be double-checked against
// the original capture. Not used anymore - see loop() for the
// dynamic equivalent: frame.buildSoundBits(78, 3, 68).
// Bytes: 33 4E B0 0F 05 -> Command=51 (Sound), Type=78, SubType=3
// (bit-shifted field, see BuOPowerlink/PowerLink.cpp), Value=68.
constexpr const char *MASTER_RADIO_SOUND_BITS = "00110011010011101011000000001111000001010001000";

// how many upcoming frames to drop because they're just our own TX
// reflected back through RX on the same bus wire
static int suppressFrames = 0;

static PLBusWriter writer(MCL_TX_PIN);
static PLBusReader reader(MCL_RX_PIN);

// One GPIO per audio source, driven HIGH for whichever source is
// currently active and LOW for all others - another project reads
// these directly (relay/transistor per pin), no decoding needed on
// its side. Placeholder pin numbers - adjust once the target board
// is picked (upesy_wrover has enough free GPIOs for a quick test;
// swap out entirely for a Stamp-class board with more headroom).
struct SourcePin { int device; gpio_num_t pin; };
constexpr SourcePin SOURCE_PINS[] = {
  {192, GPIO_NUM_4},  // TV
  {193, GPIO_NUM_5},  // Radio
  {194, GPIO_NUM_12}, // V.Aux
  {195, GPIO_NUM_13}, // A.Aux
  {197, GPIO_NUM_14}, // V.Tape
  {198, GPIO_NUM_15}, // DVD
  {202, GPIO_NUM_16}, // Sat
  {203, GPIO_NUM_17}, // PC
  {209, GPIO_NUM_18}, // A.Tape
  {210, GPIO_NUM_19}, // CD
  {211, GPIO_NUM_22}, // Phono
  {212, GPIO_NUM_23}, // A.Tape2
  {215, GPIO_NUM_27}, // CD2
};
constexpr size_t SOURCE_PIN_COUNT = sizeof(SOURCE_PINS) / sizeof(SOURCE_PINS[0]);

static void setActiveSourcePin(int device) {
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    digitalWrite(SOURCE_PINS[i].pin, SOURCE_PINS[i].device == device ? HIGH : LOW);
  }
}

// One GPIO per navigation key, same exclusive-HIGH pattern as
// SOURCE_PINS. Pin numbers are placeholders - final assignment TBD.
struct KeyPin { uint32_t key; gpio_num_t pin; };
constexpr KeyPin KEY_PINS[] = {
  {18, GPIO_NUM_32}, // Left
  {20, GPIO_NUM_33}, // Right
  {22, GPIO_NUM_2},  // Stop
};
constexpr size_t KEY_PIN_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

static void setActiveKeyPin(uint32_t key) {
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    digitalWrite(KEY_PINS[i].pin, KEY_PINS[i].key == key ? HIGH : LOW);
  }
}

// Called for every BL3500 notify with the raw Beo4 key code it
// reported (before it's mapped to a source device via +192 - Left/
// Right/Stop would otherwise collide with real source device numbers:
// 18+192=210=CD, 20+192=212=A.Tape2). Returns true if the key was
// handled here, so loop() skips the normal source-select reply for it.
// Key values are (BEO_CMD_XXX & 0x1F) from the esp32_beo4 library,
// same formula as the sources - not yet verified against real
// hardware for these three.
static bool handleBl3500Key(uint32_t key) {
  switch (key) {
    case 18: // Left  (BEO_CMD_LEFT  0x32 & 0x1F)
    case 20: // Right (BEO_CMD_RIGHT 0x34 & 0x1F)
    case 22: // Stop  (BEO_CMD_STOP  0x36 & 0x1F)
      setActiveKeyPin(key);
      return true;
    default:
      return false;
  }
}

// reply as Master would: Sound frame (only ever captured for Radio,
// reused as-is for every device) + SelectSource for the requested
// device - without this BL3500 never activates the source. Shared by
// the real notify-triggered path and the debug Serial command below.
static void sendMasterReply(int device) {
  Serial.printf("-> replying for %s(%d)\n", PLData::deviceName((uint8_t) device), device);
  suppressFrames = 2; // the 2 frames we're about to send will echo back on RX
  PLData d(""); // not parsed from a real frame - just a carrier for `device`
  d.device = device;
  //  writer.sendFrame(MASTER_RADIO_SOUND_BITS); // step-back test: known-good constant instead of buildSoundBits()
  writer.sendFrame(d.buildSoundBits(78, 3, 68)); // Type/SubType/Value from Radio's real capture
  writer.sendFrame(d.buildSelectSourceBits());
  setActiveSourcePin(device);
}

// DEBUG: type a line over Serial to test without BL3500 present.
//   "<type> <subType> <value>"  (e.g. "78 3 68") - sends a one-off
//     Sound frame with those parameters, to find out what each field
//     actually does on real hardware.
//   "<source name>" (e.g. "radio", "cd", "tv", ...) or a bare device
//     number (e.g. "193") - sends the full reply for that source, as
//     if BL3500 had just requested it.
// Not part of the normal notify-driven flow.
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

    int type, subType, value;
    if (sscanf(line.c_str(), "%d %d %d", &type, &subType, &value) == 3) {
      Serial.printf("-> debug Sound frame: type=%d subType=%d value=%d\n", type, subType, value);
      suppressFrames = 1; // this single frame will echo back on RX
      PLData dummy(""); // buildSoundBits() doesn't use any parsed frame state
      writer.sendFrame(dummy.buildSoundBits((uint8_t) type, (uint8_t) subType, (uint8_t) value));
      continue;
    }

    int device = PLData::deviceFromName(line);
    if (device < 0 && line.toInt() >= 192) device = line.toInt();
    if (device >= 0) {
      Serial.printf("-> debug: full reply for %s\n", line.c_str());
      sendMasterReply(device);
      continue;
    }

    Serial.println("debug: expected \"<type> <subType> <value>\" (e.g. \"78 3 68\") or a source name (radio, tv, cd, ...)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Beolink3500 - MCL/PL Master emulator");

  writer.begin();
  reader.begin();

  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    pinMode(SOURCE_PINS[i].pin, OUTPUT);
    digitalWrite(SOURCE_PINS[i].pin, LOW);
  }

  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    pinMode(KEY_PINS[i].pin, OUTPUT);
    digitalWrite(KEY_PINS[i].pin, LOW);
  }

  
}

void loop() {
  handleDebugSerial();

  // 1. wait (briefly - so the Serial check above stays responsive) for
  //    the next complete frame; see PLBusReader::poll
  String bits;
  if (!reader.poll(bits, pdMS_TO_TICKS(50))) return;

  // 2. our own TX reflects back onto RX via the shared bus wire - drop
  //    the N frames we know are just that echo, not real bus traffic
  if (suppressFrames > 0) {
    suppressFrames--;
    return;
  }
  Serial.printf("frame: %u bits  %s\n", bits.length(), bits.c_str());

  // 3. decode Format+AddrTo+AddrFrom+Data; bail if too short to have a header
  PLData frame(bits);
  if (!frame.valid) return;
  Serial.printf("  addrTo=%u addrFrom=%u data(%u bit)=%u\n",
                frame.addrTo, frame.addrFrom, frame.data.length(), PLData::bitsToValue(frame.data));

  // 4. only react to BL3500's own short notify (addrFrom=12, addrTo=0,
  //    data<8 bits); ignore Master traffic, other devices, long frames
  if (frame.addrFrom != PLData::BL3500_ADDR || frame.addrTo != 0 || frame.data.length() >= 8) return;

  // 5. Left/Right/Stop switch a GPIO directly instead of a source reply
  uint32_t key = PLData::bitsToValue(frame.data);
  if (handleBl3500Key(key)) return;

  // 6. notify data is a Beo4 key code (& 0x1F), already mapped to a
  //    BODev_* device by the PLData constructor; ignore unknown keys
  if (frame.device < 0) return;

  // 7. reply as Master would, and drive the matching source pin
  sendMasterReply(frame.device);
}
