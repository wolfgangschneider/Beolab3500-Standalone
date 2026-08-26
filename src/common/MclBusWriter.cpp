#include "MclBusWriter.hpp"

MclBusWriter::MclBusWriter(gpio_num_t pin) : _pin(pin) {}

void MclBusWriter::begin() {
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

  
}

void MclBusWriter::sendSoundSetup(uint8_t subType, uint8_t value) {
  sendFrame(MclData::buildSoundSetupBits(78, subType, value)); // subType=6 => value range 0-72, 72=100% - only known true for MK1, not verified for MK2
  sendVol(1); // gives MK1 more power
}

void MclBusWriter::sendSource(uint8_t device, uint8_t track) {

  String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);

  // old  send 4 frames but it looks it wor with 2 now (
  //there was an issue that the display was one behind the source selection but now it looks it works with 2 frames)
  // also a handmade send from esp was not recognized on BL 
  //sendSound(6, 68); // 3=SubType, 68=Value - matches the real Master's observed values for Radio (see git history)
  //sendSound(6, 68); // repeat to match the real Master's observed order (Sound,Audio,Sound,Audio)
  //sendFrame(select);
  //sendFrame(select);

  sendSoundSetup(6, 68); // subType=6 => value range 0-72, 72=100% - only known true for MK1, not verified for MK2
  sendFrame(select);



  // for MK2 this is enought but for uniformity i'll send same as MK1
  // sendFrame(select);
   //pulse(1);
}

void MclBusWriter::sendVol(uint8_t value) {
  sendFrame(MclData::buildSoundBits(76, 128, value)); // MK2 only, see MclBusWriter.hpp
  pulse(1);
}

void MclBusWriter::sendInit() {
  Serial.println("-> sending the first frame of the captured BW power-on sequence (rest currently commented out below)");
  sendFrame("0011000111100111111100000000100"); // Command=49, unrecognized, no known build formula - literal capture off BW1 (Beolink Wireless), frame 1 of the real 5-frame power-on sequence (rest below)
  pulse(1); // MKII trailing pulse - confirmed required for init only, not other frame types

  
  /*
  aptured from Belolink wireless 1
  delay(30);
  sendFrame(MclData::buildSoundBits(76, 128, 40)); // Sound (settled)
  delay(45);
  sendFrame(MclData::buildSelectSourceBits(193, 64, 4, 255)); // SelectSource: Radio (transient)
  delay(45);
  sendFrame(MclData::buildSelectSourceBits(193, 64, 0, 5)); // SelectSource: Radio (settled)
  delay(485);
  sendFrame(MclData::buildSoundBits(76, 128, 40)); // Sound (settled)
  */
}
