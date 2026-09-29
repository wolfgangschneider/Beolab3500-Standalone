#include "MclBusWriter.hpp"
#include "MclData.hpp"
constexpr uint8_t VOLUME_VALUE = 90; // from the live capture this sequence reproduces
void MclBusWriter::sendSource(uint8_t device, uint8_t track) {
  // All logging happens AFTER the frames are sent - no I/O between pulses.
  const char *used = name();

  if (_variant == MclMasterVariant::PL) {
    // Beocenter 2300 style: Sound-setup frame + 48-bit SelectSource, twice
    String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);

    sendInit();
    sendFrame(select);
    sendFrame(select); // for sending debug commands we need one more
  }
  else if (_variant == MclMasterVariant::MCL) {
    // MCL - a different, OLDER real Master, sniffed live via
    // BeoPowerlinkDisplay 2026-09 - no Sound(51) frame at all. Confirmed
    // by a clean repeated capture: sends the VOLUME + channel/track pair
    // of 40-bit SelectSource(59) frames TWICE in a row (not just once).
    // VOLUME's Seek follows the now-confirmed Seek=2*Value+40 formula
    // (34->108, 70->180, 88->216, all live captures).
    String volumeFrame = MclData::buildSelectSourceBits40(device, 72, 2 * VOLUME_VALUE + 40, VOLUME_VALUE);

    sendFrame(volumeFrame);
    sendFrame(MclData::buildSelectSourceBits40(device, 64, 0, track));
    sendFrame(volumeFrame);
    sendFrame(MclData::buildSelectSourceBits40(device, 64, 0, track));
  }
  else {
    // EX: alternates PL / MCL on every call
    counter++;
    if (counter % 2 == 0) {
      used = "EX-PL";
      String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);

      sendInit();
      sendFrame(select);
      sendFrame(select);
    } else {
      used = "EX-MCL";
      String volumeFrame = MclData::buildSelectSourceBits40(device, 72, 2 * VOLUME_VALUE + 40, VOLUME_VALUE);

      sendFrame(volumeFrame);
      sendFrame(MclData::buildSelectSourceBits40(device, 64, 0, track));
      sendFrame(volumeFrame);
      sendFrame(MclData::buildSelectSourceBits40(device, 64, 0, track));
    }
  }

  Serial.printf("   -> %s sends: source %s, track %d\n", used, MclData::deviceName(device), track);
}

// OFF - the real MCL Master's switch-off sequence (sniffed live from a
// BM4500): a VOLUME SelectSource frame, then channel/track with Seek=2
// Value=0, sent as a pair TWICE (once is not enough - only mutes).
// Fixed volume 90 (seek = 2*90+40), device Radio(193) like the capture.
// VERIFIED on real hardware for the MCL variant.
// The PL variant sends the single frame sniffed from a real PL Master
// instead (see below) - UNTESTED as TX.
void MclBusWriter::sendOff() {
  constexpr uint8_t device = 193;

  if (_variant == MclMasterVariant::MCL) {
    // verified on real hardware
    sendFrame(MclData::buildSelectSourceBits40(device, 72, 2 * VOLUME_VALUE + 40, VOLUME_VALUE));
    sendFrame(MclData::buildSelectSourceBits40(device, 64, 2, 0));
    sendFrame(MclData::buildSelectSourceBits40(device, 72, 2 * VOLUME_VALUE + 40, VOLUME_VALUE));
    sendFrame(MclData::buildSelectSourceBits40(device, 64, 2, 0));
    Serial.printf("   -> %s sends: OFF sequence\n", name());
  }
  else if (_variant == MclMasterVariant::PL) {
    // the real PL Master's off, sniffed live 2026-09: a SINGLE 48-bit
    // SelectSource frame  3B C1 60 02 00 00  = Radio, ValueType 96
    // (activate), Seek=2, Value=0. UNTESTED as TX.
    sendFrame(MclData::buildSelectSourceBits(device, 96, 2, 0));
    Serial.printf("   -> %s sends: OFF (activate seek=2 value=0) (UNTESTED)\n", name());
  }
  else {
    Serial.printf("   -> %s: no OFF sequence for this variant\n", name());
  }
}

// MK1 has no Vol feature - same "not available" as the base
void MclBusWriter::sendVol(uint8_t value) {
  BusWriter::sendVol(value);
  }

// Just the Sound-setup frame - no separate captured power-on sequence
// like MK2's. Also sent as the first part of every sendSource() call
// above, not just here.
//
// Value=90 was originally a guess ("a little bit of mystery... but we
// have no master") - now confirmed: a real Master, sniffed live via
// BeoPowerlinkDisplay, broadcasts this exact Type=78/SubType=128
// (VOLUME)/Value=90 Sound frame itself, so hardcoding 90 here matches
// real bus traffic, not just a plausible-looking guess.
void MclBusWriter::sendInit() {
  sendFrame(MclData::buildSoundSetupBits(78, 128, 90));

}

// for testing
void MclBusWriter::sendInit(uint8_t value) {
  //sendFrame(MclData::buildSoundSetupBits(78, 128, value));
 constexpr uint8_t device = 193; // Radio, matching the captured sequence
      sendFrame(MclData::buildSelectSourceBits40(device, 72, value, 0));
      sendFrame(MclData::buildSelectSourceBits40(device, 64, 2, 0));
     
}
