#pragma once

#include <Arduino.h>

// Converts between raw decoded bitstrings and the B&O MCL/PL
// "Datalink" frame structure - parsing incoming frames and building
// outgoing ones. No bus I/O (see MclBusReader/MclBusWriter for that).
//
// Frame = Format(3) + Address(to)(5) + Address(from)(4) + Data
// (manual fig. 2045-4). BL3500's notify is a short frame (data < 8
// bits): addrFrom=12 (its own bus address), addrTo=0, data =
// BEO_CMD_XXX & 0x1F (TV=0, Radio=1, CD=18 confirmed).
class PLData {
public:
  static constexpr uint32_t BL3500_ADDR  = 12;

  // parses a raw decoded bitstring into addrTo/addrFrom/data/device;
  // `valid` is false if the frame is too short to contain a full header
  explicit PLData(const String &bits);

  bool     valid    = false;
  uint32_t addrTo   = 0;
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

  // builds a 48-bit SelectSource frame (Command=59, this->device,
  // Type=96, then the same trailing bytes as Radio's verified real
  // capture). A trailing 00 00 (tried first) needed a second BL3500
  // notify to take effect; Radio's real 04 02 00 works on the first
  // try, so this is used as the template for every device.
  String buildSelectSourceBits() const;

  // Radio's exact real captured SelectSource bitstring (device=193),
  // hardcoded rather than derived from `device` - for A/B testing
  // against buildSelectSourceBits()'s formula-built equivalent, which
  // happens to produce the identical 48 bits for device=193.
  static String buildRadioSourceBits();

  // builds a 47-bit Sound frame (Command=51, type, subType, value) -
  // see PLData.cpp for the bit layout, which has undocumented gap
  // bits copied verbatim from the one real capture we have (a
  // Beocenter 2300's Radio Sound reply). Dynamic replacement for the
  // MASTER_RADIO_SOUND_BITS constant kept as a backup in main.cpp.
  String buildSoundBits(uint8_t type, uint8_t subType, uint8_t value) const;

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
