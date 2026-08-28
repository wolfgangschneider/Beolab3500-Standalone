#pragma once

#include <Arduino.h>

// Converts between raw decoded bitstrings and the B&O MCL/PL
// "Datalink" frame structure - parsing incoming frames and building
// outgoing ones. No bus I/O (see MclBusReader/MclBusWriter for that).
//
// The parsing side (the constructor's notify-header handling) and
// BL3500_ADDR/deviceName/deviceFromName are confirmed against Mk1
// only. The building side (buildSelectSourceBits/buildSoundSetupBits,
// via MclBusWriter::sendSource()) is shared as-is for MK2 too -
// confirmed on real hardware to activate BL3500 Mk2, not just Mk1.
//
// Frame = Format(3) + Address(to)(5) + Address(from)(4) + Data
// (manual fig. 2045-4) - only on BL3500's own short notify frame (data
// < 8 bits): addrFrom=12, data = BEO_CMD_XXX & 0x1F (TV=0, Radio=1,
// CD=18 confirmed). Long Command frames (Sound/Audio, 47+ bits) have no
// such header - Command sits directly at bit 0, see MclData.cpp. Note:
// "addrFrom" here is our own interpretation of what those header bits
// mean, fitted to match the manual's field layout - we haven't confirmed
// it's genuinely a bus address rather than e.g. some other Source/
// Command-shaped pairing that happens to produce the same numbers. Only
// the numeric values (12/9) and their use in loop() to distinguish
// notify types are verified, not the addrFrom naming itself.
// addrTo isn't tracked at all - every notify we've ever captured (TV,
// Radio, CD, Left, ...) had the same constant value there, so it never
// discriminated anything.
class MclData {
public:
  static constexpr uint32_t BL3500_ADDR  = 12;

  // parses a raw decoded bitstring into addrFrom/data/device; `valid` is
  // false if the frame is too short to contain a full header
  explicit MclData(const String &bits);

  bool     valid    = false;
  uint32_t addrFrom = 0;
  String   data;
  // BODev_* device this frame's data selects: data + 192 (only
  // meaningful when data is a short (<8 bit) notify). Verified by
  // cross-checking the esp32_beo4 library's BEO_CMD_* table against
  // BuOPowerlink/PowerLink.cpp's BODev_* constants: (BEO_CMD_XXX &
  // 0x1F) + 192 == BODev_XXX for every source command (TV, Radio, CD
  // hardware-verified directly against real BL3500 notify frames; the
  // rest checked by formula only). BODev_Aux(13) is the one known
  // exception, outside the 192-223 range this produces.
  int device = -1;

  // human-readable device name, only for logging (Serial.printf) -
  // not used anywhere in the protocol logic itself
  static const char* deviceName(uint8_t dev);

  // reverse of deviceName(): case-insensitive name -> BODev_* device,
  // -1 if unrecognized. Only for debug/testing (e.g. typing "radio"
  // over Serial), not part of the normal notify-driven flow.
  static int deviceFromName(const String &name);

  // builds a SelectSource/Audio frame (Command=59, device, valueType,
  // seek, value) - field layout confirmed identical for both
  // revisions (decodeAudio() in BeoPowerlinkDisplay/src/PowerLink.cpp:
  // Byte2=Device, Byte3=ValueType, Byte4=Seek, Byte5=Value), always 48
  // bit including a trailing Byte6=0x00 (undocumented, copied verbatim
  // from Radio's real capture off a Beocenter 2300 - real MK2 captures
  // were observed without it, at 40 bit; sending it on MK2 anyway is
  // being tested, not yet confirmed either way). No internal state
  // here - callers that need a fresh `value` each call generate it
  // themselves (there's currently no auto-incrementing counter
  // anywhere - see git history for why that was removed).
  static String buildSelectSourceBits(uint8_t device, uint8_t valueType, uint8_t seek, uint8_t value);


  // Radio's exact real captured SelectSource bitstring (device=193),
  // hardcoded rather than derived from `device` - for A/B testing
  // against buildSelectSourceBits()'s formula-built equivalent, which
  // happens to produce the identical 48 bits for device=193.
  static String buildRadioSourceBits();



  // builds a Sound frame (Command=51, type, subType, value), 43 bit -
  // MK2 only, confirmed PowerLink.cpp-conformant: SubType lands at
  // bit [19,27) and Value at [35,43), exactly where
  // BeoPowerlinkDisplay/src/PowerLink.cpp::decodeSound() reads them
  // (verified required for Vol to actually work). gap1/gap2 come from
  // the one real MK2 capture we have (Beolink Wireless BL's idle
  // Radio Sound, type=76 subType=128 value=40), verified to reproduce
  // that capture's bits exactly. Only type/subType/value are real
  // parameters.
  static String buildSoundBits(uint8_t type, uint8_t subType, uint8_t value);

  // Sound frame shape used as part of source setup/activation (see
  // MclBusWriter::sendSoundSetup()) - 47 bit, gap1/gap2 reverse-
  // engineered from a real capture (bytes 33 4E B0 0F 05 = 40 bit) -
  // which unit this was actually captured from (MK1? BW1/Beolink
  // Wireless?) is no longer known; earlier comments here claimed MK1,
  // that's now in doubt and unconfirmed either way. NOT
  // PowerLink.cpp-conformant: its SubType field lands 3 bits later
  // than PowerLink.cpp's fixed [19,27) read position, and its Value
  // field's low bits + the trailing bit extend past bit 40, i.e.
  // beyond the actual 40-bit real capture. Confirmed on real MK1
  // hardware as part of the source-setup sequence.
  static String buildSoundSetupBits(uint8_t type, uint8_t subType, uint8_t value);

  // buildSoundSetupBits2 - faithful reconstruction of the real
  // BeoCenter/BeoSound 2300 Sound frame, from live captures 2026-08-28
  // (passive GPIO34 tap between a BS2300 and a Beolab, decoded by
  // BeoPowerlinkDisplay's sniffer). 47 bit, PowerLink.cpp-conformant
  // field positions:
  //   Command(8)=51 | Type(8) | gap1(3)="101" | SubType(8) |
  //   field2(8) | Value(8) | trailing(4)="0000"
  // vs buildSoundSetupBits()'s gap1="101100" (3 bits too long), which
  // shifts SubType to [22:30) and Value to [38:46) and locks the
  // decoded volume to ~32-41. Here SubType lands at [19:27) and Value
  // at [35:43), exactly where PowerLink.cpp::decodeSound() and a real
  // BL3500 read them - and Value is the real absolute volume (matched
  // the BS2300 front-panel number: 34, 46, 74..80 across captures).
  // field2 [27:35): for SubType=128 (VOLUME) the real master sends
  // 2*Value+40 (held across 7 captures - a redundant second volume
  // encoding). Its meaning for other SubTypes is unknown (no
  // captures), so pass SubType=128 for volume; other SubTypes get a
  // 2*Value+40 field2 that may be wrong. buildSoundSetupBits() (v1) is
  // kept unchanged for A/B comparison and rollback.
  static String buildSoundSetupBits2(uint8_t type, uint8_t subType, uint8_t value);

  // converts a bit string ("1011...") to its unsigned value, MSB first
  static uint32_t bitsToValue(const String &s);

private:
  static constexpr size_t FORMAT_BITS    = 3;
  static constexpr size_t ADDR_TO_BITS   = 5;
  static constexpr size_t ADDR_FROM_BITS = 4;
  static constexpr size_t HEADER_BITS    = FORMAT_BITS + ADDR_TO_BITS + ADDR_FROM_BITS;

  // appends the 8 bits of `v`, MSB first, to `bits`
  static void appendByte(String &bits, uint8_t v);
};
