#pragma once

#include <Arduino.h>

// Transmits frames on the B&O MCL/PL "Datalink" bus by bit-banging
// GPIO timing (see PLBusReader for the receive side). Per B&O MCL-2
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
//   PLBusReader's decode).
class PLBusWriter {
public:
  explicit PLBusWriter(gpio_num_t pin);

  void begin();

  // sends bits ("1011...") framed as AGC + Start + data + Stop
  void sendFrame(const String &bits);

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
