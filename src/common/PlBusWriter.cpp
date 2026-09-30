#include "PlBusWriter.hpp"
#include "MclData.hpp"

// Drives the MK2 mute pin: on = muted (LOW), off = released (HIGH). Every
// pin write is logged on its own line, like MCL's "-> output X: GPIOn".
// Must not be called between the pulses of a frame (no I/O during TX).
void PlBusWriter::mute(bool on) {
  if (_mutePin == GPIO_NUM_NC) return;
  digitalWrite(_mutePin, on ? LOW : HIGH);
  Serial.printf("   -> output mute: GPIO%d %s\n", (int) _mutePin, on ? "LOW" : "HIGH");
}

// confirmed on real MK2 hardware as-is
void PlBusWriter::sendSource(uint8_t device, uint8_t track) {
  sendFrame(MclData::buildSelectSourceBits(device, 96, 0x00, 0));
  pulse(1); // MKII trailing pulse - confirmed required
  sendFrame(MclData::buildSelectSourceBits(device, 96, 0x00, track));
  pulse(1); // MKII trailing pulse - confirmed required
  Serial.printf("   -> PL sends: source %s, track %d\n", MclData::deviceName(device), track);
  
}

// MK2 only, see BusWriter.hpp. Sound frame with Type=76, SubType=128
// fixed - the confirmed "volume" shape (see git history: gap2+Value
// together form a single 16-bit counter, +1282 per real Vol+ press).
void PlBusWriter::sendVol(uint8_t value) {
  sendFrame(MclData::buildSoundBits(76, 128, 0));
  pulse(1); // MKII trailing pulse - confirmed required
  sendFrame(MclData::buildSoundBits(76, 128, value));
  pulse(1); // MKII trailing pulse - confirmed required
  Serial.printf("   -> PL sends: volume %d (only display)\n", value);
  //sendInit();
}

// OFF - on real MK2 hardware this switches ONLY THE DISPLAY off, not the
// system. The single frame sniffed from a real PL Master
// (3B C1 60 02 00 00 = Radio, activate, Seek=2, Value=0, same as
// MclBusWriter::sendOff's PL variant), plus the trailing pulse every MK2
// frame type needs.
void PlBusWriter::sendOff() {
  sendFrame(MclData::buildSelectSourceBits(193, 96, 2, 0));
  pulse(1); // MKII trailing pulse
  Serial.println("   -> PL sends: OFF (activate seek=2 value=0) (only display)");
}

// Replays (part of) the captured power-on sequence (captured off
// Beolink Wireless BL). Currently sends just the first frame
// (Command=49, unrecognized, no known build formula - literal
// capture) - the rest of the real 5-frame sequence is written but
// commented out below, not currently sent.
void PlBusWriter::sendInit() {

  sendFrame("0011000111100111111100000000100"); // Command=49, unrecognized, no known build formula - literal capture off BW1 (Beolink Wireless), frame 1 of the real 5-frame power-on sequence (rest below)
  pulse(1); // MKII trailing pulse - confirmed required for init only, not other frame types
  Serial.println("   -> PL sends: init"); // after the last pulse
  sendMute(0); // mute pin released (HIGH) after init

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
