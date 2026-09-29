#include "SerialDebugCommands.hpp"
#include "MclData.hpp"

// TEMP QCC-AT - forward "AT..." lines to a QCC5124 on UART1. Compiled
// out automatically if src/common/QccAtBridge.hpp is deleted.
// DISABLED by default - build with -D QCC_AT_ENABLE to turn it back on.
#if defined(QCC_AT_ENABLE) && __has_include("QccAtBridge.hpp")
#include "QccAtBridge.hpp"
#define QCC_AT_BRIDGE_PRESENT 1
#endif

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

#ifdef QCC_AT_BRIDGE_PRESENT
    if (QccAtBridge::handleLine(line)) continue;  // TEMP QCC-AT
#endif

    Serial.printf("[cmd] %s\n", line.c_str()); // trigger: typed command

    String lower = line;
    lower.toLowerCase();

    // POC (UNVERIFIED - no captured reference frame): "ALL STANDBY".
    // Beo4 ALL(0x0F) source + STANDBY(0x0C) command, sent as a full
    // SelectSource-length frame (see MclData::buildBeo4CommandBits),
    // plus the same trailing pulse the other Pl frames use. If real
    // hardware ignores this, revert the POC commit - nothing else
    // depends on it.
    if (lower == "standby" || lower == "alloff" || lower == "allstandby") {
      const uint8_t cmd = 0x0C, src = 0x01;
      _writer->sendFrame("0011101111000001010010000111000000100100");
      _writer->sendFrame("0011101111000001010000000000001000000000");
      //_writer->sendFrame(MclData::buildBeo4CommandBits(cmd, src));
      //_writer->pulse(1);
      Serial.printf("   -> %s sends: ALL STANDBY (cmd=0x%02X src=0x%02X)\n", _writer->name(), cmd, src); // log only after TX - no I/O during timed send
      continue;
    }

    // POC (test, UNVERIFIED) - reproduces a real captured MCL "off"
    // sequence from the older Master (sniffed live via
    // BeoPowerlinkDisplay 2026-09): the same VOLUME SelectSource frame
    // as a normal source-select, but the channel/track frame carries
    // Seek=2/Value=0 instead of the usual transient/settled pattern -
    // that combination was seen right as the Master was turned off.
    // Device hardcoded to Radio(193), matching the capture; whether
    // this actually powers anything down on real MK1 hardware is not
    // confirmed.
    if (lower == "off") {
      constexpr uint8_t device = 193; // Radio, matching the captured sequence
      _writer->sendFrame(MclData::buildSelectSourceBits40(device, 72, 180, 90));
      _writer->sendFrame(MclData::buildSelectSourceBits40(device, 64, 2, 0));
      Serial.printf("   -> %s sends: OFF sequence (POC, unverified)\n", _writer->name()); // log only after TX - no I/O during timed send
      continue;
    }

    if (lower == "init") {
      //digitalWrite(_mk2MutePin, LOW); // ensure mute is off during the init sequence when display is needed
      _writer->sendInit();
      //digitalWrite(_mk2MutePin, HIGH); // ensure mute is off after the init sequence
      Serial.printf("   -> %s sends: init\n", _writer->name()); // log only after TX - no I/O during timed send
      continue;
    }

    // requires a literal space ("init180" is not "init 180"), same
    // reasoning as "vol " below. Calls the sendInit(value) overload
    // (see BusWriter.hpp/MclBusWriter.cpp) to probe a value other than
    // the revision's hardcoded default.
    if (lower.startsWith("init ")) {
      int initValue = line.substring(5).toInt();
      //digitalWrite(_mk2MutePin, LOW);
      _writer->sendInit((uint8_t) initValue);
      //digitalWrite(_mk2MutePin, HIGH);
      Serial.printf("   -> %s sends: init value=%d\n", _writer->name(), initValue); // log only after TX - no I/O during timed send
      continue;
    }

    // requires a literal space ("vol5" is not "vol 5") - consistent
    // with the source-name dispatch below, which also needs a space to
    // split a name from its track (some names, e.g. "cd2"/"a.tape2",
    // already end in a digit, so "cd25" alone couldn't be split
    // unambiguously into name+track).
    if (lower.startsWith("vol ")) {
      int volValue = line.substring(4).toInt();
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
