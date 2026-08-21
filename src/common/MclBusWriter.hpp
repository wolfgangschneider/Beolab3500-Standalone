#pragma once

#include <Arduino.h>
#include "BL3500Version.hpp"
#include "MclData.hpp"

// Transmits frames on the B&O MCL/PL "Datalink" bus by bit-banging
// GPIO timing (see MclBusReader for the receive side). Per B&O MCL-2
// Service Manual ("Datalink '86"):
// - The transmitting unit pulls the line LOW for a fixed-width strobe
//   then releases it; the timing symbol (t1..t5) is the FULL period
//   (LOW+HIGH), not the LOW width alone.
// - Timing symbols: t1=3.125ms t2=6.250ms t3=9.375ms (data),
//   t4=12.500ms (Stop), t5=15.625ms (Start). Start is preceded by two
//   AGC-priming t1 pulses (manual fig. 2045-4: "1 1 5") - a real
//   receiver's analog front-end needs these to lock on.
// - Differential bit code: which timing symbol encodes a bit depends
//   on both the bit and the previously sent bit (mirrors
//   MclBusReader's decode).
//
// Shared by both Beolab 3500 revisions - confirmed identical t1..t5
// timing/differential bit encoding on the wire (see main.cpp). The
// one confirmed difference, gated on `version`:
// real Mk2 Master (Beolink Wireless BL) traffic always has one extra
// low strobe pulse after Stop, required for BL3500 Mk2 to activate
// from our own TX - Mk1 traffic never has it.
class MclBusWriter {
public:
  explicit MclBusWriter(gpio_num_t pin, BL3500Version version = BL3500Version::MK1);

  void begin();

  // sends bits ("1011...") framed as AGC + Start + data + Stop (+ the
  // trailing pulse if this writer was constructed with version=MK2)
  void sendFrame(const String &bits);

  // Higher-level protocol sends, moved here from main.cpp so both
  // main.cpp and (later) other callers can reuse them - all branch
  // internally on this writer's own `_version` instead of taking it
  // as a parameter. None of these touch GPIO (e.g. the downstream
  // active-source-pin indicator) - that stays the caller's job in
  // main.cpp, since it's a hardware-output concern unrelated to the
  // bus itself.

  // reply as Master would: Sound frame(s) + SelectSource for the
  // requested device - without this BL3500 never activates the
  // source. MK1: Sound,Sound,SelectSource,SelectSource (matches the
  // real Master's observed order). MK2, EXPERIMENTAL: single
  // SelectSource only, not yet confirmed to activate BL3500 Mk2 on
  // its own. `track` is the SelectSource frame's Value byte - a plain
  // caller-supplied value, no internal counter.
  void sendSource(uint8_t device, uint8_t track);

  // sends a single Sound frame, Type fixed to 78 - subType/value are
  // the caller's actual parameters.
  void sendSound(uint8_t subType, uint8_t value);

  // MK2 only: Sound frame with Type=76, SubType=128 fixed - the
  // confirmed "volume" shape (see git history: gap2+Value together
  // form a single 16-bit counter, +1282 per real Vol+ press). `value`
  // is the caller's actual parameter. No-ops with a log line if this
  // writer isn't MK2.
  void sendVol(uint8_t value);

  // MK2 only: replays (part of) the captured power-on sequence.
  // Currently sends just the first frame (Command=49, unrecognized,
  // no known build formula - literal capture) - the rest of the real
  // 5-frame sequence is written but commented out in MclBusWriter.cpp,
  // not currently sent. No-ops with a log line if this writer isn't MK2.
  void sendInit();

private:
  static constexpr uint32_t T1_US = 3125;
  static constexpr uint32_t T2_US = 6250;
  static constexpr uint32_t T3_US = 9375;
  static constexpr uint32_t T4_US = 12500; // Stop
  static constexpr uint32_t T5_US = 15625; // Start
  static constexpr uint32_t STROBE_LOW_US = 1562;

  gpio_num_t _pin;
  BL3500Version _version;

  // pulls the bus LOW for the fixed strobe width, then releases it
  // for the rest of the target timing symbol's period
  void pulse(uint8_t tcode);
  // which timing symbol encodes `bit` given the previous bit
  static uint8_t encodeBit(int lastBit, int bit);
};
