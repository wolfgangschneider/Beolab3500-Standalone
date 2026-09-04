#pragma once

#include <Arduino.h>
#include "MclData.hpp"

// GPIO outputs shared by both Beolab 3500 revisions (confirmed identical
// wire protocol, see common/BusReader.hpp) - drives a separate, not-yet-built downstream board
// (source-switch matrix + resistor-ladder button emulator). No bus
// protocol knowledge here; callers translate their own protocol's
// device/key values before calling in.
namespace GpioOutputs {

// One GPIO per audio source, driven HIGH for whichever source is
// currently active and LOW for all others - another project reads
// these directly (relay/transistor per pin), no decoding needed on
// its side. Placeholder pin numbers, board-specific (#ifdef in
// GpioOutputs.cpp) - see the pin table in README.md.
struct SourcePin { int device; gpio_num_t pin; };
extern const SourcePin SOURCE_PINS[];
extern const size_t SOURCE_PIN_COUNT;

// pinMode(OUTPUT) + idle LOW for every entry in SOURCE_PINS. MK1 only -
// on MK2 these pins are never configured as outputs (and some, like
// GPIO43 on m5_stamp_S3, double as MK2_DETECTED - driving them would
// be wrong there), so callers must gate setActiveSourcePin() on
// blVersion==MK1 themselves (see main-standalone.cpp's loop() and
// SerialDebugCommands).
void beginSourcePins();
void setActiveSourcePin(int device);

// One GPIO per navigation key (Left/Right/Stop). Board-specific
// (#ifdef in GpioOutputs.cpp, matching SOURCE_PINS) - esp32_wrover MUST
// NOT use GPIO6-11: those are hard-wired to the module's internal SPI
// flash (CLK/SD0/SD1/SD2/SD3/CMD), which the CPU executes code from
// (memory-mapped). Reconfiguring GPIO7/GPIO9 as plain GPIO (as this
// used to do) glitches the flash bus the CPU is actively fetching
// instructions from - crashes before any Serial output is even
// possible (no panic dump - the panic handler itself can't be fetched
// from the now-broken flash), escalating into outright "flash read
// err" boot failures after a few such cycles. Confirmed the hard way:
// commenting out GpioOutputs::beginKeyPins() was what stopped the
// bootloop. m5_stamp_S3 (ESP32-S3, different flash pinout entirely)
// isn't affected and keeps GPIO7/9.
extern const gpio_num_t KEY_PIN_LEFT;
extern const gpio_num_t KEY_PIN_RIGHT;
extern const gpio_num_t KEY_PIN_STOP;
extern const gpio_num_t KEY_PINS[];
extern const size_t KEY_PIN_COUNT;

// idle floating (INPUT, open switch) for every entry in KEY_PINS
void beginKeyPins();

// Simulates a momentary pushbutton on a resistor-ladder KEY input
// (e.g. a Bluetooth module like the MH-M18: one analog pin, each
// button = a different resistor to GND) - idle is floating (INPUT,
// open switch), a press is briefly OUTPUT+LOW (closed switch to GND),
// then back to floating. Plain digitalWrite(HIGH/LOW) does NOT work
// here: a driven HIGH is a real 3.3V on the node, not the high-Z
// "disconnected" state an open button has, and would throw off the
// resistor-ladder voltage division. Forces every other KEY_PINS entry
// back to floating first (in case one was ever left stuck as an
// output).
void pressKey(gpio_num_t pin);

// MK1 only: BL3500's own remote sends Left/Right/Stop as short notify
// frames too (same shape as a source-select notify) - recognizes them
// by frame.addrFrom + frame.data and drives the matching KEY_PINS[]
// pin via pressKey() above instead of the usual source-select reply.
// Returns true if it was a nav key and has been handled (so the
// caller should skip its normal reply for this frame).
bool handleNavKeys(const MclData &frame);

}
