#include "Beo4KeysOnMK1.hpp"
#include "GpioOutputs.hpp"

// Key values are (BEO_CMD_XXX & 0x1F) from the esp32_beo4 library, same
// formula as the sources - not yet verified against real hardware for
// these three. Left/Right/Stop would otherwise collide with real
// source device numbers once +192 is applied (18+192=210=CD,
// 20+192=212=A.Tape2) - the addrFrom check above is what disambiguates
// a real CD/A.Tape2 source request from a nav key press.
bool Beo4KeysOnMK1::handle(uint32_t addrFrom, uint32_t key) {
  if (addrFrom != NAV_KEY_ADDR) return false;

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
