#include "GpioOutputs.hpp"

namespace GpioOutputs {

 
  //
  // Pin numbers are board-specific (#ifdef, matching src/main.cpp's
  // MCL_TX_PIN/MCL_RX_PIN/MK2_MUTE_PIN block) - they used to be one
  // fixed list (4,5,17,19) that happened to collide with the Stamp S3's
  // bus pins (GPIO5 = MK2_MUTE_PIN) and referenced GPIO17/19, which that
  // module doesn't even break out (see docs.m5stack.com/en/core/StampS3
  // - only G0-15 and G39-46 are exposed).


    // placeholders - final assignment TBD.
  // Attention this can changed from version to version !!!!
#if defined(BOARD_M5STAMP_S3)
const SourcePin SOURCE_PINS[] = {
  {192, GPIO_NUM_44},  // TV
  {193, GPIO_NUM_43},  // Radio
 // {194, GPIO_NUM_9},  // V.Aux
 // {195, GPIO_NUM_10}, // A.Aux
 // {197, GPIO_NUM_11}, // V.Tape
 // {198, GPIO_NUM_15}, // DVD
 // {202, GPIO_NUM_39}, // Sat
 // {203, GPIO_NUM_7},  // PC
 // {209, GPIO_NUM_40}, // A.Tape
 // {210, GPIO_NUM_14},  // CD
 // {211, GPIO_NUM_9},  // Phono
 // {212, GPIO_NUM_41}, // A.Tape2
 // {215, GPIO_NUM_42}, // CD2
};
#else // BOARD_WROOVER
const SourcePin SOURCE_PINS[] = {
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
 // {211, GPIO_NUM_22}, // Phono
 // {212, GPIO_NUM_23}, // A.Tape2
 // {215, GPIO_NUM_27}, // CD2
};
#endif
const size_t SOURCE_PIN_COUNT = sizeof(SOURCE_PINS) / sizeof(SOURCE_PINS[0]);

void beginSourcePins() {
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    pinMode(SOURCE_PINS[i].pin, OUTPUT);
    digitalWrite(SOURCE_PINS[i].pin, LOW);
  }
}

void setActiveSourcePin(int device) {
  const SourcePin *match = nullptr;
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    bool active = SOURCE_PINS[i].device == device;
    digitalWrite(SOURCE_PINS[i].pin, active ? HIGH : LOW);
    if (active) match = &SOURCE_PINS[i];
  }
  if (match) Serial.printf("-> source %s: GPIO%d\n", MclData::deviceName((uint8_t) device), (int) match->pin);
  else Serial.printf("-> source %s\n", MclData::deviceName((uint8_t) device));
}

const gpio_num_t KEY_PINS[] = {KEY_PIN_LEFT, KEY_PIN_RIGHT, KEY_PIN_STOP};
const size_t KEY_PIN_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

void beginKeyPins() {
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    pinMode(KEY_PINS[i], INPUT); // idle floating (open switch)
  }
}

void pressKey(gpio_num_t pin) {
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    if (KEY_PINS[i] != pin) pinMode(KEY_PINS[i], INPUT);
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(100);
  pinMode(pin, INPUT);
}

namespace {
  // Sender address navigation-key notify frames were observed to use,
  // distinct from MclData::BL3500_ADDR (12) used for source-select
  // notifies. Not otherwise identified (same open question as the
  // still-unexplained address 11 seen elsewhere) - checking it is what
  // separates navigation keys from source keys that alias to the same
  // Beo4 value (see the case values below).
  constexpr uint32_t NAV_KEY_ADDR = 9;
}

// Key values are (BEO_CMD_XXX & 0x1F) from the esp32_beo4 library, same
// formula as the sources - not yet verified against real hardware for
// these three. Left/Right/Stop would otherwise collide with real
// source device numbers once +192 is applied (18+192=210=CD,
// 20+192=212=A.Tape2) - the addrFrom check is what disambiguates a
// real CD/A.Tape2 source request from a nav key press.
bool handleNavKeys(const MclData &frame) {
  if (frame.addrFrom != NAV_KEY_ADDR) return false;
  uint32_t key = MclData::bitsToValue(frame.data);

  gpio_num_t pin;
  switch (key) {
    case 18: pin = KEY_PIN_LEFT;  break; // Left  (BEO_CMD_LEFT  0x32 & 0x1F)
    case 20: pin = KEY_PIN_RIGHT; break; // Right (BEO_CMD_RIGHT 0x34 & 0x1F)
    case 22: pin = KEY_PIN_STOP;  break; // Stop  (BEO_CMD_STOP  0x36 & 0x1F)
    default: return false;
  }
  Serial.printf("-> key %u: GPIO%d pressed\n", key, (int) pin);
  pressKey(pin);
  return true;
}

}
