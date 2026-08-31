#pragma once

#include <Arduino.h>

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

// pinMode(OUTPUT) + idle LOW for every entry in SOURCE_PINS
void beginSourcePins();
void setActiveSourcePin(int device);

// One GPIO per navigation key (Left/Right/Stop). Same fixed values for
// both esp32_wrover and m5_stamp_S3 (no per-board #ifdef needed, both
// boards have these free). On m5_stamp_S3 only, LEFT/STOP happen to
// land on the same pins as main.cpp's MK2_MUTE_PIN(5)/MK2_BL_MUTE_PIN(9)
// - safe because loop()'s mute-mirror code is gated to blVersion==MK2
// only, while beginKeyPins()/pressKey() only ever run when
// blVersion==MK1, so the two purposes never touch the pin in the same
// running mode. On esp32_wrover, MK2_MUTE_PIN/MK2_BL_MUTE_PIN are 26/33
// - no overlap with these pins there at all.
// On m5_stamp_S3, RIGHT(7) also reuses SOURCE_PINS' commented-out PC
// entry (GPIO7) - that one is NOT mode-exclusive (both KEY_PINS and
// SOURCE_PINS are MK1-only and active *simultaneously* within that
// mode), it's only safe while PC stays disabled there. Uncommenting PC
// on GPIO7 in GpioOutputs.cpp would collide with this - give RIGHT a
// different free pin first if that ever happens. (On esp32_wrover, PC
// is already active on GPIO17, not GPIO7 - no collision to worry about
// there.)
constexpr gpio_num_t KEY_PIN_LEFT  = GPIO_NUM_5;
constexpr gpio_num_t KEY_PIN_RIGHT = GPIO_NUM_7;
constexpr gpio_num_t KEY_PIN_STOP  = GPIO_NUM_9;
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

}
