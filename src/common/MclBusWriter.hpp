#pragma once

#include "BusWriter.hpp"

// MK1: BL3500's real Master is a Beocenter 2300, speaking the B&O
// MCL/PL "Datalink" bus. begin()/sendFrame()/pulse() are inherited
// as-is from BusWriter (not overridden - see BusWriter.hpp for why).
// sendSource()/sendInit() have real MK1-specific content (see
// MclBusWriter.cpp); sendVol() just forwards to BusWriter's "not
// available" default, since MK1 has no Vol feature. sendInit(value)
// is an extra non-virtual overload, not currently called from
// main.cpp.
class MclBusWriter : public BusWriter {
public:
  explicit MclBusWriter(gpio_num_t pin) : BusWriter(pin) {}

  void sendSource(uint8_t device, uint8_t track) override;

  void sendVol(uint8_t value) override;
  void sendInit() override;
  void sendInit(uint8_t value) ;
};
