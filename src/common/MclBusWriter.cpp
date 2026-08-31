#include "MclBusWriter.hpp"

// confirmed on real MK1 hardware as-is
void MclBusWriter::sendSource(uint8_t device, uint8_t track) {
  String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);
  
  sendInit();
  sendFrame(select);

  // very old but working
  //sendSound(6, 68); // 3=SubType, 68=Value - matches the real Master's observed values for Radio (see git history)
  //sendSound(6, 68); // repeat to match the real Master's observed order (Sound,Audio,Sound,Audio)
  //sendFrame(select);
  //sendFrame(select);
 
}

// MK1 has no Vol feature - same "not available" as the base
void MclBusWriter::sendVol(uint8_t value) {
  //test  
  sendFrame(MclData::buildSoundBits(76, 128, value));
  BusWriter::sendVol(value);
}

// MK1's "init" is just its normal source-setup call - no separate
// captured power-on sequence like MK2's. Radio(193) as a default test
// device, matching the same default used elsewhere for MK2 testing.
//// a little bit of mystery - it has to do with the vol from master - but we have no master

void MclBusWriter::sendInit() {
  sendFrame(MclData::buildSoundSetupBits(78, 128, 90));
  
}

// for testing
void MclBusWriter::sendInit(uint8_t value) {
  sendFrame(MclData::buildSoundSetupBits(78, 128, value));
  
}
