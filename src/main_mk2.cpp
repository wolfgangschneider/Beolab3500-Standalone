/*
  Beolab3500-Standalone - MK2 PowerLink Master emulator (not yet
  implemented)

  The Beolab 3500 Mk2 uses a different bus protocol than the Mk1: pure
  PowerLink, not the MCL/PL "Datalink" protocol MK1 uses (see
  main_mk1.cpp / mcl_mk1/) - different frame structure and timing, not
  yet reverse engineered.

  This is a placeholder entry point so `pio run -e upesy_wrover_mk2`
  builds cleanly. Fill in powerlink_mk2/ (PlBusReader/PlBusWriter/
  PlData, mirroring the mcl_mk1/ pattern) as the protocol gets
  analyzed, then wire it up here the same way main_mk1.cpp does for
  MK1 - no code should be shared between the two protocol
  implementations beyond src/common/.
*/

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Beolab3500-Standalone MK2 (PowerLink) - not implemented yet");
}

void loop() {
}
