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
 // {194, GPIO_NUM_12}, // V.Aux
 // {195, GPIO_NUM_13}, // A.Aux
 // {197, GPIO_NUM_14}, // V.Tape
 // {198, GPIO_NUM_15}, // DVD
 // {202, GPIO_NUM_16}, // Sat
  {203, GPIO_NUM_17}, // PC
 // {209, GPIO_NUM_18}, // A.Tape
  {210, GPIO_NUM_19}, // CD
  {211, GPIO_NUM_22}, // Phono
 // {212, GPIO_NUM_23}, // A.Tape2
 // {215, GPIO_NUM_27}, // CD2
};
constexpr size_t SOURCE_PIN_COUNT = sizeof(SOURCE_PINS) / sizeof(SOURCE_PINS[0]);

static void setActiveSourcePin(int device) {
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    digitalWrite(SOURCE_PINS[i].pin, SOURCE_PINS[i].device == device ? HIGH : LOW);
  }
}

// Sender address navigation-key notify frames were observed to use,
// distinct from BL3500_ADDR (12) used for source-select notifies. Not
// otherwise identified (same open question as the still-unexplained
// address 11 seen elsewhere) - only used here to separate navigation
// keys from source keys that alias to the same Beo4 value (see
// handleBl3500Key() call site in loop() for the specific collisions).
constexpr uint32_t NAV_KEY_ADDR = 9;

// One GPIO per navigation key, pin numbers are placeholders - final
// assignment TBD. Only used to pinMode() them in setup(); the actual
// pulse-on-press logic is inline in handleBl3500Key() below.
constexpr gpio_num_t KEY_PIN_LEFT  = GPIO_NUM_12;
constexpr gpio_num_t KEY_PIN_RIGHT = GPIO_NUM_13;
constexpr gpio_num_t KEY_PIN_STOP  = GPIO_NUM_14;
constexpr gpio_num_t KEY_PINS[] = {KEY_PIN_LEFT, KEY_PIN_RIGHT, KEY_PIN_STOP};
constexpr size_t KEY_PIN_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

// Called for every BL3500 notify with the raw Beo4 key code it
// reported (before it's mapped to a source device via +192 - Left/
// Right/Stop would otherwise collide with real source device numbers:
// 18+192=210=CD, 20+192=212=A.Tape2). Returns true if the key was
// handled here, so loop() skips the normal source-select reply for it.
// Key values are (BEO_CMD_XXX & 0x1F) from the esp32_beo4 library,
// same formula as the sources - not yet verified against real
// hardware for these three. Simulates a momentary pushbutton on a
// resistor-ladder KEY input (e.g. a Bluetooth module like the MH-M18:
// one analog pin, each button = a different resistor to GND) - idle
// is floating (INPUT, open switch), a press is briefly OUTPUT+LOW
// (closed switch to GND), then back to floating. Unlike SOURCE_PINS,
// this can't just be digitalWrite HIGH/LOW: a driven HIGH is a real
// 3.3V on the node, not the high-Z "disconnected" an open button is,
// and would throw off the resistor-ladder voltage division.
static bool handleBl3500Key(uint32_t key) {
  gpio_num_t pin;
  switch (key) {
    case 18: pin = KEY_PIN_LEFT;  break; // Left  (BEO_CMD_LEFT  0x32 & 0x1F)
    case 20: pin = KEY_PIN_RIGHT; break; // Right (BEO_CMD_RIGHT 0x34 & 0x1F)
    case 22: pin = KEY_PIN_STOP;  break; // Stop  (BEO_CMD_STOP  0x36 & 0x1F)
    default: return false;
  }
  Serial.printf("-> key %u: GPIO%d pressed\n", key, (int) pin);
  // every other key pin is forced back to floating (in case one was
  // ever left stuck as an output), then the target pin briefly becomes
  // an output driving LOW (closed switch to GND) before returning to
  // floating (open switch) - plain digitalWrite HIGH/LOW doesn't work
  // here: a driven HIGH is a real 3.3V on the node, not the same as
  // "disconnected", and would throw off the resistor-ladder voltage
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    if (KEY_PINS[i] != pin) pinMode(KEY_PINS[i], INPUT);
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(100);
  pinMode(pin, INPUT);
  return true;
}

// reply as Master would: Sound frame + SelectSource for the requested
// device - without this BL3500 never activates the source. Takes the
// parsed frame directly (not just its .device) so there's one source of
// truth read at the point of use. Only for the real notify path - the
// debug Serial command below has no real parsed frame, so it builds its
// reply inline instead of faking one here.
static void sendMasterReply(const PLData &frame) {
  uint8_t device = (uint8_t) frame.device;
  //Serial.printf("-> replying for %s(%d)\n", PLData::deviceName(device), device);
  //delay(10); // give BL3500 a moment to finish its own TX before we start ours -
             // explicit rather than relying on Serial.printf's own timing, which
             // isn't there anymore now that the print above is commented out
  // Sound/SelectSource/Sound/SelectSource, alternating - matches the real
  // Master's observed order (Sound,Audio,Sound,Audio). The old "2x Sound
  // then 1x SelectSource" order never got BL3500 to actually switch.
  String test = PLData::buildSelectSourceBits(device);
 
  writer.sendFrame(PLData::buildSoundBits(78, 6, 68)); // is the range 6 => 0-72  that means 72 = 100% (3 >= 32=100%) // last parameter unknown 
  writer.sendFrame(PLData::buildSoundBits(78, 6, 68));
  writer.sendFrame(test);
  writer.sendFrame(test);
 // writer.sendFrame(PLData::buildSoundBits(78, 3, 68));
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
      writer.sendFrame(PLData::buildSoundBits((uint8_t) type, (uint8_t) subType, (uint8_t) value));
      continue;
    }

    int device = PLData::deviceFromName(line);
    if (device < 0 && line.toInt() >= 192) device = line.toInt();
    if (device >= 0) {
      // build the exact 17-bit bitstring BL3500's own notify would have
      // for this device (Format=000, AddrTo=00000, AddrFrom=BL3500_ADDR,
      // data=(device-192)&0x1F), then parse it through PLData's real
      // constructor - same object sendMasterReply() always gets, just
      // synthesized instead of coming off the bus.
      uint8_t key = (uint8_t) (device - 192) & 0x1F;
      String bits = "000"; // Format (unused by PLData)
      for (int b = 4; b >= 0; b--) bits += ((0 >> b) & 1) ? '1' : '0';        // AddrTo = 0
      for (int b = 3; b >= 0; b--) bits += ((PLData::BL3500_ADDR >> b) & 1) ? '1' : '0'; // AddrFrom
      for (int b = 4; b >= 0; b--) bits += ((key >> b) & 1) ? '1' : '0';      // data
      PLData frame(bits);
      sendMasterReply(frame);
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
    pinMode(KEY_PINS[i], INPUT); // idle floating (open switch); see handleBl3500Key()
  }

  
}

void loop() {
  // TEMP HW DEBUG: raw digitalRead of GPIO34, remove once the hardware
  // issue is sorted out
  //Serial.println(digitalRead(MCL_RX_PIN) ? "GPIO34 HIGH" : "GPIO34 LOW");

  handleDebugSerial();

  // 1. wait (briefly - so the Serial check above stays responsive) for
  //    the next complete frame; see PLBusReader::poll
  String bits;
  if (!reader.poll(bits, pdMS_TO_TICKS(50))) return;

  // 2. our own TX reflects back onto RX via the shared bus wire - still
  //    logged (so we can see what actually hit the bus, e.g. to compare
  //    against what we intended to send), but not decoded/acted on
  if (suppressFrames > 0) {
    suppressFrames--;
    Serial.printf("echo:  %u bits  %s\n", bits.length(), bits.c_str());
    return;
  }
  //Serial.printf("frame: %u bits  %s\n", bits.length(), bits.c_str());

  // 3. decode Format+AddrTo+AddrFrom+Data; bail if too short to have a header
  PLData frame(bits);
  if (!frame.valid) return;
 // Serial.printf("  addrTo=%u addrFrom=%u data(%u bit)=%u\n",
 //               frame.addrTo, frame.addrFrom, frame.data.length(), PLData::bitsToValue(frame.data));

  // 4. only react to short notify-shaped frames (addrTo=0, data<8 bits);
  //    ignore Master traffic and other long frames
  if (frame.addrTo != 0 || frame.data.length() >= 8) return;

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
  uint32_t key = PLData::bitsToValue(frame.data);
  if (frame.addrFrom == NAV_KEY_ADDR && handleBl3500Key(key)) return;

  // 6. everything else must be BL3500's own source-select notify
  //    (addrFrom=12) with a Beo4 key code (& 0x1F) already mapped to a
  //    BODev_* device by the PLData constructor; ignore anything else
  if (frame.addrFrom != PLData::BL3500_ADDR || frame.device < 0) return;

  // 7. reply as Master would, and drive the matching source pin
  sendMasterReply(frame);
}
