#include "MclBusWriter.hpp"

// confirmed on real MK1 hardware as-is
void MclBusWriter::sendSource(uint8_t device, uint8_t track) {
  String select = MclData::buildSelectSourceBits(device, 96, 0x00, track);

  sendInit();
  sendFrame(select);
  sendFrame(select); // for sending debug commands we need one more

  // very old but working
  //sendSound(6, 68); // 3=SubType, 68=Value - matches the real Master's observed values for Radio (see git history)
  //sendSound(6, 68); // repeat to match the real Master's observed order (Sound,Audio,Sound,Audio)
  //sendFrame(select);
  //sendFrame(select);
 
}

// MK1 has no Vol feature - same "not available" as the base
void MclBusWriter::sendVol(uint8_t value) {
  BusWriter::sendVol(value);
  }

// Just the Sound-setup frame - no separate captured power-on sequence
// like MK2's. Also sent as the first part of every sendSource() call
// above, not just here.
//// a little bit of mystery - it has to do with the vol from master - but we have no master

void MclBusWriter::sendInit() {

  sendFrame(MclData::buildSoundSetupBits(78, 128, 90));

}

// for testing
void MclBusWriter::sendInit(uint8_t value) {
  sendFrame(MclData::buildSoundSetupBits(78, 128, value));
  
}
