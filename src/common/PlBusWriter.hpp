#pragma once

#include "BusWriter.hpp"

// MK2: BL3500's real Master is a separate "Beolink Wireless BL" unit,
// speaking the same wire protocol under the "PL" naming (see B&O
// service manual: PL ON/PL RX/PL TX == MLCON/MCLRX/MCLTX).
// begin()/sendFrame()/pulse() are inherited as-is from BusWriter (not
// overridden - see BusWriter.hpp for why). sendSource()/sendVol()/
// sendInit() are overridden here with real MK2-specific content, each
// followed by its own confirmed trailing pulse() - see PlBusWriter.cpp
// for the exact tcode each one uses (they differ).
class PlBusWriter : public BusWriter {
public:
  explicit PlBusWriter(gpio_num_t pin) : BusWriter(pin) {}

  void sendSource(uint8_t device, uint8_t track) override;
  void sendVol(uint8_t value) override;
  void sendInit() override;
};
