#include "GpioOutputs.hpp"

namespace GpioOutputs {

// SOURCE_PINS[] / SOURCE_PIN_COUNT and KEY_PIN_LEFT/RIGHT/STOP are all
// board-specific and live in common/BoardConfig.hpp now.

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
  if (match) Serial.printf("   -> output %s: GPIO%d\n", MclData::deviceName((uint8_t) device), (int) match->pin);
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
  digitalWrite(pin, HIGH);
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
  Serial.printf("[BL] key %s\n", key == 18 ? "Left" : key == 20 ? "Right" : "Stop");
  Serial.printf("   -> output: GPIO%d pressed\n", (int) pin);
  pressKey(pin);
  return true;
}

}
