#pragma once

#include <Arduino.h>
#include "BL3500Version.hpp"

// Converts between raw decoded bitstrings and the B&O MCL/PL
// "Datalink" frame structure - parsing incoming frames and building
// outgoing ones. No bus I/O (see MclBusReader/MclBusWriter for that).
//
// The parsing side (the constructor's notify-header handling) and
// BL3500_ADDR/deviceName/deviceFromName are confirmed against Mk1
// only. The building side (buildSelectSourceBits/buildSoundBits) is
// also used from main.cpp's MK2 path as an experiment - Mk2's real
// Master traffic decodes into the same Command/Device shapes as Mk1
// (see main.cpp's file header), so whether frames *built* the Mk1 way
// also activate a Mk2 unit (as opposed to only literal captured-
// sequence replay) is being tested, not yet confirmed either way.
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
  // Byte2=Device, Byte3=ValueType, Byte4=Seek, Byte5=Value) except MK1
  // has one extra trailing Byte6=0x00 (undocumented, copied verbatim
  // from Radio's real capture off a Beocenter 2300; MK2's real
  // captures never have it - 40 bit, not 48). No internal state here -
  // callers that need a fresh `value` each call generate it themselves
  // (there's currently no auto-incrementing counter anywhere - see
  // git history for why that was removed).
  static String buildSelectSourceBits(uint8_t device, uint8_t valueType, uint8_t seek, uint8_t value, BL3500Version version = BL3500Version::MK1);


  // Radio's exact real captured SelectSource bitstring (device=193),
  // hardcoded rather than derived from `device` - for A/B testing
  // against buildSelectSourceBits()'s formula-built equivalent, which
  // happens to produce the identical 48 bits for device=193.
  static String buildRadioSourceBits();



  // builds a Sound frame (Command=51, type, subType, value) - 47 bit
  // for MK1, 43 bit for MK2 (gap bits at different positions/widths -
  // see MclData.cpp). Both gap layouts are undocumented, copied
  // verbatim from the one real capture of each revision we have (MK1:
  // a Beocenter 2300's Radio Sound reply; MK2: Beolink Wireless BL's
  // idle Radio Sound, verified to reproduce that real capture
  // bit-exactly for (76,128,40)) - only type/subType/value are real
  // parameters either way. Dynamic MK1 replacement for the
  // MASTER_RADIO_SOUND_BITS constant kept as a backup in main.cpp.
  static String buildSoundBits(uint8_t type, uint8_t subType, uint8_t value, BL3500Version version = BL3500Version::MK1);

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
