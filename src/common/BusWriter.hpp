#pragma once

#include <Arduino.h>

// Transmits frames on the B&O MCL/PL "Datalink" bus by bit-banging
// GPIO timing (see BusReader for the receive side). Per B&O MCL-2
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
//   BusReader's decode).
//
// Base class for both Beolab 3500 revisions. main-standalone.cpp holds
// a single `BusWriter *writer` chosen at startup to actually point at
// an MclBusWriter or a PlBusWriter depending on blVersion - everything
// declared here is reachable through that one pointer either way.
// begin()/sendFrame()/pulse() live ONLY here, not virtual, not
// overridden anywhere - the low-level wire framing genuinely doesn't
// differ between revisions. sendSource()/sendVol()/sendInit() are
// virtual with harmless "not available" defaults (not pure virtual -
// on purpose: that keeps BusWriter itself concretely instantiable, so
// Beolab3500-PL2PL can use it directly without pulling in MclBusWriter/
// PlBusWriter/MclData just to get a compilable type - see
// main-pl2pl.cpp, which never calls these three anyway).
class BusWriter {
public:
  explicit BusWriter(gpio_num_t pin);
  virtual ~BusWriter() = default;

  void begin();

  // sends bits ("1011...") framed as AGC + Start + data + Stop
  void sendFrame(const String &bits);

  // pulls the bus LOW for the fixed strobe width, then releases it
  // for the rest of the target timing symbol's period - public so
  // main-standalone.cpp's debug commands can send one-off extra pulses directly
  void pulse(uint8_t tcode);

  // reply as Master would: SelectSource for the requested device -
  // without this BL3500 never activates the source. `track` is the
  // SelectSource frame's Value byte - a plain caller-supplied value,
  // no internal counter. See MclBusWriter.cpp / PlBusWriter.cpp for
  // what each revision actually sends. Default here just logs "not
  // available".
  virtual void sendSource(uint8_t device, uint8_t track);

  // MK2 feature: Sound frame with Type=76, SubType=128 fixed - the
  // confirmed "volume" shape (see git history: gap2+Value together
  // form a single 16-bit counter, +1282 per real Vol+ press). `value`
  // is the caller's actual parameter. Default here just logs "not
  // available".
  virtual void sendVol(uint8_t value);

  // Sends an activation sequence appropriate for the revision - see
  // MclBusWriter.cpp (MK1: just the Sound-setup frame) and
  // PlBusWriter.cpp (MK2: the captured power-on frame) for what each
  // actually does. Default here just logs "not available".
  virtual void sendInit();

protected:
  static constexpr uint32_t T1_US = 3125;
  static constexpr uint32_t T2_US = 6250;
  static constexpr uint32_t T3_US = 9375;
  static constexpr uint32_t T4_US = 12500; // Stop
  static constexpr uint32_t T5_US = 15625; // Start
  static constexpr uint32_t STROBE_LOW_US = 1562;

  gpio_num_t _pin;

  // which timing symbol encodes `bit` given the previous bit
  static uint8_t encodeBit(int lastBit, int bit);
};
