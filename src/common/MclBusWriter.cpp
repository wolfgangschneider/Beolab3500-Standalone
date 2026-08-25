#include "MclBusWriter.hpp"

MclBusWriter::MclBusWriter(gpio_num_t pin) : _pin(pin) {}

void MclBusWriter::begin(BL3500Version version) {
  _version = version;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW); // idle: transistor off, bus released
}

void MclBusWriter::pulse(uint8_t tcode) {
  uint32_t period = tcode == 1 ? T1_US : tcode == 2 ? T2_US : tcode == 3 ? T3_US : tcode == 4 ? T4_US : T5_US;
  digitalWrite(_pin, HIGH); // transistor on -> bus pulled low
  delayMicroseconds(STROBE_LOW_US);
  digitalWrite(_pin, LOW);  // transistor off -> bus released, pull-up brings it back high
  delayMicroseconds(period - STROBE_LOW_US);
}

uint8_t MclBusWriter::encodeBit(int lastBit, int bit) {
  if (lastBit == 0) return bit ? 3 : 2;
  return bit ? 2 : 1;
}

void MclBusWriter::sendFrame(const String &bits) {
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
  if (_version == BL3500Version::MK2) pulse(1); // see MclBusWriter.hpp
}

void MclBusWriter::sendSound(uint8_t subType, uint8_t value) {
  sendFrame(MclData::buildSoundBits(78, subType, value, _version)); // is the range 6 => 0-72  that means 72 = 100% (3 >= 32=100%) // last parameter unknown
}

void MclBusWriter::sendSource(uint8_t device, uint8_t track) {
  if (_version == BL3500Version::MK2) {
    sendFrame(MclData::buildSelectSourceBits(device, 64, 0, track, BL3500Version::MK2));
    return;
  }

  // MK1: Sound/SelectSource/Sound/SelectSource, alternating - matches
  // the real Master's observed order (Sound,Audio,Sound,Audio). The
  // old "2x Sound then 1x SelectSource" order never got BL3500 to
  // actually switch.
  String select = MclData::buildSelectSourceBits(device, 96, 0x00, track, BL3500Version::MK1);
  sendSound(6, 68); // 3=SubType, 68=Value - matches the real Master's observed values for Radio (see git history)
  sendSound(6, 68); // repeat to match the real Master's observed order (Sound,Audio,Sound,Audio)
  sendFrame(select);
  sendFrame(select);
}

void MclBusWriter::sendVol(uint8_t value) {
  if (_version != BL3500Version::MK2) {
    Serial.println("-> sendVol() only available for MK2");
    return;
  }
  sendFrame(MclData::buildSoundBits(76, 128, value, _version));
}

void MclBusWriter::sendInit() {
  if (_version != BL3500Version::MK2) {
    Serial.println("-> sendInit() only available for MK2");
    return;
  }
  Serial.println("-> sending the first frame of the captured BW power-on sequence (rest currently commented out below)");
  sendFrame("0011000111100111111100000000100"); // unknown: Command=49, unrecognized - see main.cpp's file header
  /*
  delay(30);
  sendFrame(MclData::buildSoundBits(76, 128, 40, BL3500Version::MK2)); // Sound (settled)
  delay(45);
  sendFrame(MclData::buildSelectSourceBits(193, 64, 4, 255, BL3500Version::MK2)); // SelectSource: Radio (transient)
  delay(45);
  sendFrame(MclData::buildSelectSourceBits(193, 64, 0, 5, BL3500Version::MK2)); // SelectSource: Radio (settled)
  delay(485);
  sendFrame(MclData::buildSoundBits(76, 128, 40, BL3500Version::MK2)); // Sound (settled)
  */
}
