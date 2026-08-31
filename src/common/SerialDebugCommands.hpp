#pragma once

#include <Arduino.h>
#include "BusWriter.hpp"
#include "BL3500Version.hpp"

// Debug Serial command line - lets you test the bus without a real
// Master present (call poll() every loop() tick). Not part of MK1's
// normal notify-driven flow; MK2 has no automatic flow at all, so
// this is its only way to send anything besides the boot sequence in
// main.cpp's setup(). Commands:
//   "init"        - calls writer->sendInit() (see MclBusWriter.cpp /
//                    PlBusWriter.cpp for what each revision sends)
//   "vol <value>" - calls writer->sendVol(value) (MK2 only feature -
//                    MK1's writer just logs "not available")
//   "<source name>" (e.g. "radio", "cd", "tv", ...) or a bare device
//     number (e.g. "193"), optionally followed by a track value (e.g.
//     "radio 4", default 0) - calls writer->sendSource(device, track),
//     as if BL3500 had just requested it.
class SerialDebugCommands {
public:
  // `writer`/`blVersion` are bound by reference to main.cpp's globals -
  // `writer` itself is reassigned once, in setup(), after this is
  // constructed, so capturing it by value here would go stale.
  SerialDebugCommands(BusWriter *&writer, BL3500Version &blVersion, gpio_num_t mk2MutePin)
    : _writer(writer), _blVersion(blVersion), _mk2MutePin(mk2MutePin) {}

  void poll();

private:
  BusWriter *&_writer;
  BL3500Version &_blVersion;
  gpio_num_t _mk2MutePin;
  String _buf; // accumulates a line across poll() calls, so slow typing never times out
};
