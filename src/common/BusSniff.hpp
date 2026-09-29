#pragma once

#include <Arduino.h>
#include "MclData.hpp"

// Listen-only decoder for the real Master's long frames (Sound / SelectSource
// / unknown), logged as "[MASTER] ..." so a real Master (e.g. BM4500) on the
// bus can be watched next to our own "[cmd]/[BL]" triggers. Field offsets
// are the same as BeoPowerlinkDisplay/src/Sniff.hpp (verified there).
// Short frames are logged raw as "[RX short]"; loop() adds its "[BL] ..." line
// when it recognises a BL notify.
namespace BusSniff {

// short notify (<=24 bit) vs. long command frame
static constexpr unsigned MIN_COMMAND_BITS = 25;

inline unsigned bitsToUint(const String &s) {
  unsigned v = 0;
  for (unsigned i = 0; i < s.length(); i++) v = (v << 1) | (s[i] == '1' ? 1 : 0);
  return v;
}

// MSB-first slice, -1 unless [a,b) lies fully inside `bits`
inline long field(const String &bits, int a, int b) {
  if (a < 0 || b <= a || (unsigned) b > bits.length()) return -1;
  return (long) bitsToUint(bits.substring(a, b));
}

inline const char *soundSubTypeName(long sub) {
  switch (sub) {
    case 128: case 135: return "VOLUME";
    case 129: return "BALANCE";
    case 130: return "BASS";
    case 131: return "TREBLE";
    case 132: return "LOUDNESS";
    case 144: return "MUTE";
  }
  return "?";
}

// " | hex: 33 4E B0 ..." - full bytes, a trailing <8-bit remainder in [..]
inline void printHex(const String &bits) {
  Serial.print(" | hex:");
  const unsigned n = bits.length();
  for (unsigned i = 0; i < n; i += 8) {
    String chunk = bits.substring(i, i + 8);
    if (chunk.length() == 8) Serial.printf(" %02X", bitsToUint(chunk));
    else                     Serial.printf(" [%s]", chunk.c_str());
  }
}

// verbose = also show short/garbled captures (e.g. the 1-bit "0" the
// Master's trailing pulse leaves behind) and the empty-capture count.
// Off by default; toggled by the "verbose" Serial command.
inline bool &verbose() { static bool v = false; return v; }

// Logs every frame the reader delivers (own-TX echoes must be filtered out
// before calling): decoded when the Command is known, otherwise raw bits +
// hex - short, garbled and unknown frames are shown, never dropped.
// Only empty (0 bit) captures are counted and reported with the next
// frame, since glitches on a noisy bus would otherwise flood the log.
inline void logFrame(const String &bits) {
  static uint32_t lastMs = 0;
  static uint32_t emptySkipped = 0;
  const unsigned n = bits.length();

  if (n == 0) { emptySkipped++; return; }
  // short/garbled captures: only on request (see verbose())
  if (n < MIN_COMMAND_BITS && !verbose()) return;
  if (emptySkipped && verbose()) {
    Serial.printf("[RX] %lu empty capture(s) skipped\n", (unsigned long) emptySkipped);
    emptySkipped = 0;
  }
  if (!verbose()) emptySkipped = 0;

  // gap since the previous frame, to see repeats and their rhythm
  const uint32_t now = millis();
  Serial.printf("[+%lums] ", (unsigned long) (lastMs ? now - lastMs : 0));
  lastMs = now;

  // short frame: BL notify or something we don't know - raw. loop() adds
  // its own "[BL] ..." line when it recognises the notify.
  if (n < MIN_COMMAND_BITS) {
    Serial.printf("[RX short] %u bit: %s", n, bits.c_str());
    printHex(bits);
    Serial.println();
    return;
  }

  const unsigned cmd = bitsToUint(bits.substring(0, 8));

  if (cmd == 51) {
    long sub = field(bits, 19, 27), val = field(bits, 35, 43);
    // only the PL-style Master (Beocenter 2300) sends a Sound frame
    Serial.printf("[MASTER PL] Sound %s = %ld  (%u bit)", soundSubTypeName(sub), val, n);
  } else if (cmd == 59) {
    long dev = field(bits, 8, 16), vt = field(bits, 16, 24);
    long seek = field(bits, 24, 32), val = field(bits, 32, 40);
    const char *what = vt == 72 ? "volume" : vt == 64 ? "channel/track" : vt == 96 ? "activate" : "?";
    // style inferred from ValueType (not confirmed): 72/64 = MCL-style
    // volume + channel/track pair, 96 = PL-style activate
    const char *style = (vt == 72 || vt == 64) ? "MCL" : vt == 96 ? "PL" : "?";
    Serial.printf("[MASTER %s] SelectSource %s: %s seek=%ld value=%ld  (%u bit)",
                  style, MclData::deviceName((uint8_t) dev), what, seek, val, n);
  } else {
    Serial.printf("[MASTER ?] unknown cmd %u  (%u bit)", cmd, n);
  }
  printHex(bits);
  Serial.println();
}

}  // namespace BusSniff
