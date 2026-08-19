#include "PLData.hpp"

// splits the bitstring into Format(3)+AddrTo(5)+AddrFrom(4)+Data, then
// - if Data looks like a BL3500 notify (short, <8 bit) - resolves it
// to the BODev_* device it selects (= data + 192, see PLData.hpp for
// how this was derived), so callers don't have to.
PLData::PLData(const String &bits) {
  if (bits.length() <= HEADER_BITS) return; // too short for a header; valid stays false

  addrTo   = bitsToValue(bits.substring(FORMAT_BITS, FORMAT_BITS + ADDR_TO_BITS));
  addrFrom = bitsToValue(bits.substring(FORMAT_BITS + ADDR_TO_BITS, HEADER_BITS));
  data     = bits.substring(HEADER_BITS);
  valid = true;

  if (data.length() < 8) device = (int) bitsToValue(data) + 192;
}

// only for Serial.printf - device numbers below are the raw BODev_*
// addresses (BuOPowerlink/PowerLink.cpp), not used for any logic
const char* PLData::deviceName(uint8_t dev) {
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

int PLData::deviceFromName(const String &name) {
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

// builds a 48-bit SelectSource frame. Trailing bytes are Radio's real
// captured values, sniffed off the bus from a Beocenter 2300 (our
// Master), reused as-is for every device - a trailing 00 00 (tried
// first) needed a second BL3500 notify to take effect, Radio's real
// value works on the first try.
String PLData::buildSelectSourceBits(uint8_t device) {
  uint8_t bytes[6] = {
    59,      // Byte1 = Command: 59 = Audio
    device,  // Byte2 = Device: the BODev_* source being selected
    96,               // Byte3 = ValueType: 96 = SelectSource (not in the
                       // documented 64=channel/72=volume table - our own
                       // find, but confirmed working). Tried 64 (the
                       // documented "Channel select" ValueType, matching
                       // real CD1/ATap2/RadC3/RadC4 captures) + Value=4 as
                       // a test - did NOT activate the source, reverted.
    0x04,             // Byte4: undocumented anywhere, meaning unknown
    0x02,             // Byte5 = Value: shown as a channel/station number
                       // (DisplayBase::PrintAudio) - Radio's real capture,
                       // reused as-is; likely device-specific, not general
    0x00,             // Byte6: not read/used by BuOPowerlink/PowerLink.cpp at all
  };
  String bits;
  for (int i = 0; i < 6; i++) appendByte(bits, bytes[i]);
  return bits;
}

// exact raw bitstring from the real capture (device=193=Radio),
// verbatim, not byte-reconstructed - kept for A/B testing against
// buildSelectSourceBits() with device=193, which produces this exact
// same 48 bits via the formula instead.
String PLData::buildRadioSourceBits() {
  return "001110111100000101100000000001000000001000000000";
}

// 47 bits = Command(8) + Type(8) + gap1(6) + SubType(8) + gap2(8) +
// Value(8) + trailing(1). gap1/gap2/trailing come from the one real
// capture we have (Beocenter 2300, Radio's Sound reply: bytes 33 4E
// B0 0F 05, i.e. type=78 subType=3 value=68) and are copied verbatim
// since their meaning isn't documented anywhere - only type/subType/
// value are real parameters here.
String PLData::buildSoundBits(uint8_t type, uint8_t subType, uint8_t value) {
  const char *gap1     = "101100";
  const char *gap2     = "11000001";
  const char  trailing = '0';

  String bits;
  appendByte(bits, 51); // Command = Sound
  appendByte(bits, type);
  bits += gap1;
  appendByte(bits, subType);
  bits += gap2;
  appendByte(bits, value);
  bits += trailing;
  return bits;
}

void PLData::appendByte(String &bits, uint8_t v) {
  for (int b = 7; b >= 0; b--) bits += ((v >> b) & 1) ? '1' : '0';
}

// MSB-first: "1011" -> 0b1011 = 11
uint32_t PLData::bitsToValue(const String &s) {
  uint32_t v = 0;
  for (size_t i = 0; i < s.length(); i++) {
    v = (v << 1) | (s[i] == '1' ? 1 : 0);
  }
  return v;
}
