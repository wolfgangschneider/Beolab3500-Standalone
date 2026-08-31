#pragma once

#include <Arduino.h>

// MK1 only: BL3500's own remote sends Left/Right/Stop as short notify
// frames too (same shape as a source-select notify) - these three
// drive a KEY_PINS[] output directly (see GpioOutputs.hpp) instead of
// getting the usual source-select reply.
class Beo4KeysOnMK1 {
public:
  // Called from loop() for every decoded notify frame's addrFrom/key.
  // Returns true if it was a nav key and has been handled (so loop()
  // should skip its normal source-select reply for this frame).
  static bool handle(uint32_t addrFrom, uint32_t key);

private:
  // Sender address navigation-key notify frames were observed to use,
  // distinct from MclData::BL3500_ADDR (12) used for source-select
  // notifies. Not otherwise identified (same open question as the
  // still-unexplained address 11 seen elsewhere) - checking it is what
  // separates navigation keys from source keys that alias to the same
  // Beo4 value (see handle()'s case values in the .cpp).
  static constexpr uint32_t NAV_KEY_ADDR = 9;
};
