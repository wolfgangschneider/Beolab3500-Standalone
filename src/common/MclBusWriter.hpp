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
// timing/differential bit encoding on the wire (see main.cpp), and
// sendFrame() itself is identical for both. The one confirmed Mk2
// difference lives in sendInit(), not sendFrame(): the captured Mk2
// power-on frame needs one extra low strobe pulse after Stop to
// activate BL3500 Mk2 from our own TX - other frame types (Sound,
// SelectSource, Vol) don't need it.
class MclBusWriter {
public:
  explicit MclBusWriter(gpio_num_t pin);

  void begin();

  // sends bits ("1011...") framed as AGC + Start + data + Stop
  void sendFrame(const String &bits);

  // Higher-level protocol sends, moved here from main.cpp so both
  // main.cpp and (later) other callers can reuse them. This class
  // doesn't track a BL3500Version - sendSource() always builds the
  // same bit shapes for both revisions now (MclData::
  // buildSelectSourceBits(), MclData::buildSpecialSoundBits() via
  // sendSound() below). Confirmed on real MK1 hardware; sendVol()'s
  // separate PowerLink.cpp-conformant MclData::buildSoundBits() call
  // is unaffected and confirmed still working on MK2. None of these
  // touch GPIO (e.g. the downstream active-source-pin indicator) -
  // that stays the caller's job in main.cpp, since it's a
  // hardware-output concern unrelated to the bus itself.

  // reply as Master would: Sound frame(s) + SelectSource for the
  // requested device - without this BL3500 never activates the
  // source. Sound,Sound,SelectSource,SelectSource (matches the real
  // MK1 Master's observed order; reused as-is for MK2 - confirmed on
  // real hardware to activate BL3500 Mk2 too). `track` is the
  // SelectSource frame's Value byte - a plain caller-supplied value,
  // no internal counter.
  void sendSource(uint8_t device, uint8_t track);

  // sends a single Sound frame (MclData::buildSpecialSoundBits() shape
  // - confirmed working on real MK1 hardware), Type fixed to 78 -
  // subType/value are the caller's actual parameters.
  void sendSound(uint8_t subType, uint8_t value);

  // MK2 only, caller's responsibility to not call this for MK1: Sound
  // frame with Type=76, SubType=128 fixed - the confirmed "volume"
  // shape (see git history: gap2+Value together form a single 16-bit
  // counter, +1282 per real Vol+ press). `value` is the caller's
  // actual parameter.
  void sendVol(uint8_t value);

  // MK2 only, caller's responsibility to not call this for MK1:
  // replays (part of) the captured power-on sequence. Currently sends
  // just the first frame (Command=49, unrecognized, no known build
  // formula - literal capture) - the rest of the real 5-frame
  // sequence is written but commented out in MclBusWriter.cpp, not
  // currently sent.
  void sendInit();

private:
  static constexpr uint32_t T1_US = 3125;
  static constexpr uint32_t T2_US = 6250;
  static constexpr uint32_t T3_US = 9375;
  static constexpr uint32_t T4_US = 12500; // Stop
  static constexpr uint32_t T5_US = 15625; // Start
  static constexpr uint32_t STROBE_LOW_US = 1562;

  gpio_num_t _pin;

  // pulls the bus LOW for the fixed strobe width, then releases it
  // for the rest of the target timing symbol's period
  void pulse(uint8_t tcode);
  // which timing symbol encodes `bit` given the previous bit
  static uint8_t encodeBit(int lastBit, int bit);
};
