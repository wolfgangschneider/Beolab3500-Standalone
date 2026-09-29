#include "PlBusWriter.hpp"
#include "MclData.hpp"

// confirmed on real MK2 hardware as-is
void PlBusWriter::sendSource(uint8_t device, uint8_t track) {
  String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);
  sendFrame(select);
  pulse(1); // MKII trailing pulse - confirmed required
  Serial.printf("   -> PL sends: source %s, track %d\n", MclData::deviceName(device), track);
}

// MK2 only, see BusWriter.hpp. Sound frame with Type=76, SubType=128
// fixed - the confirmed "volume" shape (see git history: gap2+Value
// together form a single 16-bit counter, +1282 per real Vol+ press).
void PlBusWriter::sendVol(uint8_t value) {
  sendFrame(MclData::buildSoundBits(76, 128, value));
  pulse(1); // MKII trailing pulse - confirmed required
  Serial.printf("   -> PL sends: volume %d\n", value);
}

// Replays (part of) the captured power-on sequence (captured off
// Beolink Wireless BL). Currently sends just the first frame
// (Command=49, unrecognized, no known build formula - literal
// capture) - the rest of the real 5-frame sequence is written but
// commented out below, not currently sent.
void PlBusWriter::sendInit() {

  //digitalWrite(_mk2MutePin, LOW); // ensure mute is off during the init sequence when display is needed
  sendFrame("0011000111100111111100000000100"); // Command=49, unrecognized, no known build formula - literal capture off BW1 (Beolink Wireless), frame 1 of the real 5-frame power-on sequence (rest below)
  pulse(1); // MKII trailing pulse - confirmed required for init only, not other frame types
  //digitalWrite(_mk2MutePin, HIGH); // ensure mute is off after the init sequence

  /*
  captured from Beolink Wireless 1
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
