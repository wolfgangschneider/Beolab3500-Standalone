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

  // remember for echo detection (after the last pulse - nothing timed left)
  _txBits[_txIdx] = bits;
  _txMs[_txIdx] = millis();
  _txIdx = (_txIdx + 1) % ECHO_SLOTS;
}

bool BusWriter::consumeEcho(const String &bits) {
  const uint32_t now = millis();
  for (int i = 0; i < ECHO_SLOTS; i++) {
    if (_txMs[i] && now - _txMs[i] < 1500 && _txBits[i] == bits) {
      _txMs[i] = 0; // each sent frame explains exactly one echo
      return true;
    }
  }
  return false;
}

void BusWriter::sendSource(uint8_t device, uint8_t track) {
  Serial.println("   -> sendSource() not available on this writer");
}

void BusWriter::sendVol(uint8_t value) {
  Serial.println("   -> sendVol() only available for MK2");
}

void BusWriter::sendInit() {
  Serial.println("   -> sendInit() not available on this writer");
}

// POC (UNVERIFIED - no captured reference frame): "ALL STANDBY". Beo4
// ALL(0x0F) source + STANDBY(0x0C) command, as two literal SelectSource-
// length frames (the variant currently tried by hand, src=0x01; see
// MclData::buildBeo4CommandBits for the built form, currently unused).
// Doesn't switch off the MCL Master setup ("off" does) - not yet tried on PL.
void BusWriter::sendStandby() {
  sendFrame("0011101111000001010010000111000000100100");
  sendFrame("0011101111000001010000000000001000000000");
  //sendFrame(MclData::buildBeo4CommandBits(0x0C, 0x01));
  //pulse(1);
  Serial.printf("   -> %s sends: ALL STANDBY\n", name());
}

void BusWriter::sendMute(bool on) {
  Serial.println("   -> sendMute() only available for MK2");
}

void BusWriter::sendOff() {
  Serial.println("   -> sendOff() not available on this writer");
}

void BusWriter::sendInit(uint8_t value) {
  Serial.println("   -> sendInit(value) not available on this writer");
}
