#include "SerialDebugCommands.hpp"
#include "MclData.hpp"

void SerialDebugCommands::poll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      _buf += c;
      continue;
    }
    if (_buf.length() == 0) continue; // ignore a bare \r\n pair / empty line

    String line = _buf;
    _buf = "";
    line.trim();

    String lower = line;
    lower.toLowerCase();

    if (lower == "init") {
     
      digitalWrite(_mk2MutePin, LOW); // ensure mute is off during the init sequence when display is needed
      _writer->sendInit();
      digitalWrite(_mk2MutePin, HIGH); // ensure mute is off after the init sequence
      continue;
    }

    // requires a literal space ("vol5" is not "vol 5") - consistent
    // with the source-name dispatch below, which also needs a space to
    // split a name from its track (some names, e.g. "cd2"/"a.tape2",
    // already end in a digit, so "cd25" alone couldn't be split
    // unambiguously into name+track).
    if (lower.startsWith("vol ")) {
      int volValue = line.substring(4).toInt();
      Serial.printf("-> debug Vol frame: value=%d\n", volValue);
      _writer->sendVol((uint8_t) volValue);
      continue;
    }

    // "radio"/"radio <track>" is NOT handled above - it falls through
    // to the shared source-name+track dispatch below (same as MK1),
    // calling _writer->sendSource(193, track).

    // "<source name>" or "<source name> <track>" (e.g. "radio 4") -
    // track defaults to 0 if not given.
    String nameToken = line;
    int track = 0;
    int spaceIdx = line.indexOf(' ');
    if (spaceIdx >= 0) {
      nameToken = line.substring(0, spaceIdx);
      track = line.substring(spaceIdx + 1).toInt();
    }
    int device = MclData::deviceFromName(nameToken);
    if (device < 0 && nameToken.toInt() >= 192) device = nameToken.toInt();
    if (device >= 0) {
      _writer->sendSource((uint8_t) device, (uint8_t) track);
      continue;
    }

    Serial.println("debug: expected \"init\", \"vol <value>\", or a source name (radio, tv, cd, ...), optionally followed by a track value (e.g. \"radio 4\")");
  }
}
