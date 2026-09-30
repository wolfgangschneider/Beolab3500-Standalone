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
  // `mutePin` is optional (GPIO_NUM_NC = none): the MK2 mute output, driven
  // by sendMute() ("mute 1|0" command).
  explicit PlBusWriter(gpio_num_t pin, gpio_num_t mutePin = GPIO_NUM_NC)
      : BusWriter(pin), _mutePin(mutePin) {
    if (_mutePin != GPIO_NUM_NC) pinMode(_mutePin, OUTPUT);
  }

  const char* name() const override { return "PL"; }
  void sendSource(uint8_t device, uint8_t track) override;
  void sendVol(uint8_t value) override;
  void sendInit() override;
  void sendOff() override; // MK2: switches only the display off
  void sendMute(bool on) override { mute(on); } // logs "-> output mute: GPIOn LOW/HIGH"

private:
  gpio_num_t _mutePin;
  void mute(bool on); // see PlBusWriter.cpp
};
