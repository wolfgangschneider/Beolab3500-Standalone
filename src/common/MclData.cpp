#include "MclData.hpp"

// The Format(3)+AddrTo(5)+AddrFrom(4) header only exists on BL3500's own
// short notify frame (17 bits total: 12-bit header + 5-bit data). Long
// Command frames (Sound/Audio, 47+ bits) have NO header - Command sits
// directly at bit 0. Confirmed by decoding many real Master captures both
// ways: skipping a header on long frames always produced garbage (random
// addrTo/addrFrom, no recognizable Command byte), never skipping gave
// clean, repeatedly-verified Command/Device values (e.g. 59=Audio,
// 51=Sound, correct BODev_* device numbers). Applying the header-skip
// unconditionally (as this used to do) left addrTo/addrFrom full of
// meaningless noise for every long frame - never used for any decision
// (loop()'s frame-type filter also checks data.length()), but wasted a
// lot of debugging time before that was caught. addrTo isn't extracted
// at all anymore - every real notify had the same constant value there.
MclData::MclData(const String &bits) {
  constexpr size_t NOTIFY_MAX_BITS = 20; // 17-bit notify + a little margin

  if (bits.length() <= HEADER_BITS) return; // too short for a header; valid stays false

  if (bits.length() <= NOTIFY_MAX_BITS) {
    addrFrom = bitsToValue(bits.substring(FORMAT_BITS + ADDR_TO_BITS, HEADER_BITS));
    data     = bits.substring(HEADER_BITS);
  } else {
    data = bits; // no header - Command starts at bit 0
  }
  valid = true;

  if (data.length() < 8) device = (int) bitsToValue(data) + 192;
}

// only for Serial.printf - device numbers below are the raw BODev_*
// addresses (BuOPowerlink/PowerLink.cpp), not used for any logic
const char* MclData::deviceName(uint8_t dev) {
  switch (dev) {
    case 192: return "TV";
    case 193: return "Radio";
    case 194: return "V.Aux";
    case 195: return "A.Aux";
    case 197: return "V.Tape";
    case 198: return "DVD";
    case 202: return "Sat";
    case 203: return "PC";
    case 209: return "A.Tape";
    case 210: return "CD";
    case 211: return "Phono";
    case 212: return "A.Tape2";
    case 215: return "CD2";
  }
  return "unknown";
}

int MclData::deviceFromName(const String &name) {
  String n = name;
  n.toLowerCase();
  if (n == "tv")      return 192;
  if (n == "radio")   return 193;
  if (n == "v.aux")   return 194;
  if (n == "a.aux")   return 195;
  if (n == "v.tape")  return 197;
  if (n == "dvd")     return 198;
  if (n == "sat")     return 202;
  if (n == "pc")      return 203;
  if (n == "a.tape")  return 209;
  if (n == "cd")      return 210;
  if (n == "phono")   return 211;
  if (n == "a.tape2") return 212;
  if (n == "cd2")     return 215;
  return -1;
}

// Command(8) + Device(8) + ValueType(8) + Seek(8) + Value(8), byte-
// aligned, straight from decodeAudio()'s confirmed field layout - same
// for both revisions, except MK1 has one extra trailing Byte6=0x00
// (undocumented, copied verbatim from Radio's real capture off a
// Beocenter 2300; MK2's real captures never have it - 40 bit, not 48).
String MclData::buildSelectSourceBits(uint8_t device, uint8_t valueType, uint8_t seek, uint8_t value) {
  String bits;
  appendByte(bits, 59); // Command = Audio/SelectSource
  appendByte(bits, device);
  appendByte(bits, valueType);
  appendByte(bits, seek);
  appendByte(bits, value);
  //if (version == BL3500Version::MK1) 
  appendByte(bits, 0x00); // Byte6, MK1 only
  return bits;
}

// exact raw bitstring from the real capture (device=193=Radio),
// verbatim, not byte-reconstructed - kept for A/B testing against
// buildSelectSourceBits() with device=193, which produces this exact
// same 48 bits via the formula instead.
String MclData::buildRadioSourceBits() {
  return "001110111100000101100000000001000000001000000000";
}

// 43 bits = Command(8) + Type(8) + gap1(3) + SubType(8) + gap2(8) +
// Value(8), no trailing bit. gap1/gap2 come from the one real MK2
// capture we have (Beolink Wireless BL, idle Radio Sound: type=76
// subType=128 value=40), verified to reproduce that capture's bits
// exactly. gap1=3 puts SubType at [19,27) and Value at [35,43) -
// bit-for-bit where PowerLink.cpp::decodeSound() reads them (confirmed
// required for Vol to actually work).
String MclData::buildSoundBits(uint8_t type, uint8_t subType, uint8_t value) {
  String bits;
  appendByte(bits, 51); // Command = Sound
  appendByte(bits, type);
  bits += "101"; // gap1
  appendByte(bits, subType);
  bits += "10001100"; // gap2
  appendByte(bits, value);
  return bits;
}

// Superseded by buildSoundSetupBits() below - kept only for A/B
// comparison, not called from anywhere anymore. 47 bits =
// Command(8) + Type(8) + gap1(6) +
// SubType(8) + gap2(8) + Value(8) + trailing(1). gap1/gap2/trailing
// reverse-engineered from a real capture (bytes 33 4E B0 0F 05, 40
// bit) - which unit this was actually captured from (MK1? BW1/Beolink
// Wireless?) is no longer known; earlier comments here claimed MK1,
// that's now in doubt and unconfirmed either way. NOT
// PowerLink.cpp-conformant (see buildSoundBits()) - gap1=6 puts
// SubType at [22,30), 3 bits later than PowerLink.cpp's fixed [19,27)
// read. Value's low bits and the trailing bit also fall past bit 40,
// i.e. beyond the real 40-bit capture. Confirmed on real MK1 hardware
// as part of the source-setup sequence.

String MclData::buildSoundSetupBits_old(uint8_t type, uint8_t subType, uint8_t value) {
  String bits;
  appendByte(bits, 51); // Command = Sound
  appendByte(bits, type);
  bits += "101100"; // gap1
  appendByte(bits, subType);
  bits += "11000001"; // gap2
  appendByte(bits, value);
  bits += '0'; // trailing
  return bits;
}

// Faithful reconstruction of the real BS2300 Sound frame - see the long
// note in MclData.hpp. 47 bit, PowerLink.cpp-conformant field positions:
// Command(8)=51 + Type(8) + gap1(3)="101" + SubType(8) + field2(8) +
// Value(8) + trailing(4)="0000". For SubType=128 (VOLUME): Value is the
// real absolute volume, field2 = 2*Value+40 (the master's redundant
// second volume encoding, verified across 7 captures). 2*value+40 fits a
// byte for value up to 107 (real volume is <= ~90); it wraps past that.
String MclData::buildSoundSetupBits(uint8_t type, uint8_t subType, uint8_t value) {
  String bits;
  appendByte(bits, 51); // Command = Sound
  appendByte(bits, type);
  bits += "101"; // gap1 (3 bits - v1's "101100" was 3 too long)
  appendByte(bits, subType);
  appendByte(bits, (uint8_t) (2 * value + 40)); // field2 [27:35), volume: 2*Value+40
  appendByte(bits, value);
  bits += "0000"; // trailing (4 bits)
  return bits;
}

void MclData::appendByte(String &bits, uint8_t v) {
  for (int b = 7; b >= 0; b--) bits += ((v >> b) & 1) ? '1' : '0';
}

// MSB-first: "1011" -> 0b1011 = 11
uint32_t MclData::bitsToValue(const String &s) {
  uint32_t v = 0;
  for (size_t i = 0; i < s.length(); i++) {
    v = (v << 1) | (s[i] == '1' ? 1 : 0);
  }
  return v;
}
