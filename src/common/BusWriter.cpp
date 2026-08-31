#include "BusWriter.hpp"

BusWriter::BusWriter(gpio_num_t pin) : _pin(pin) {}

void BusWriter::begin() {
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW); // idle: transistor off, bus released
}

void BusWriter::pulse(uint8_t tcode) {
  uint32_t period = tcode == 1 ? T1_US : tcode == 2 ? T2_US : tcode == 3 ? T3_US : tcode == 4 ? T4_US : T5_US;
  digitalWrite(_pin, HIGH); // transistor on -> bus pulled low
  delayMicroseconds(STROBE_LOW_US);
  digitalWrite(_pin, LOW);  // transistor off -> bus released, pull-up brings it back high
  delayMicroseconds(period - STROBE_LOW_US);
}

uint8_t BusWriter::encodeBit(int lastBit, int bit) {
  if (lastBit == 0) return bit ? 3 : 2;
  return bit ? 2 : 1;
}

void BusWriter::sendFrame(const String &bits) {
  int lastBit = 1; // Start acts as an implicit "1" reference, matching the RX side
  pulse(1); // AGC
  pulse(1); // AGC
  pulse(5); // Start
  for (size_t i = 0; i < bits.length(); i++) {
    int bit = bits[i] - '0';
    pulse(encodeBit(lastBit, bit));
    lastBit = bit;
  }
  pulse(4); // Stop
}

void BusWriter::sendVol(uint8_t value) {
  Serial.println("-> sendVol() only available for MK2");
}
