#pragma once

// ═════════════════════════════════════════════════════════════════════
//  TEMPORARY - QCC5124 AT-command bridge over the USB serial monitor.
//
//  Any line typed into the serial monitor that starts with "AT" is
//  forwarded verbatim (+ CR) to the QCC5124 on UART1; everything the
//  QCC5124 sends back is echoed straight to the USB serial monitor.
//  Non-AT lines are left untouched for the normal debug-command
//  dispatch (SerialDebugCommands.cpp).
//
//  ---- HOW TO REMOVE (all hooks are guarded by __has_include of this
//       file, so deleting it is enough to compile the feature out) ----
//    1. delete this file (src/common/QccAtBridge.hpp)
//    2. optionally delete the three  // TEMP QCC-AT  guarded blocks in
//       src/main-standalone.cpp and src/common/SerialDebugCommands.cpp
//
//  ---- WIRING (HYT/HNYATX QCC module, 3V3 logic, cross TX/RX, common
//       GND; module also needs its own VCC = 3.6..5.5 V) ----
//    ESP32 GPIO QCC_AT_TX_PIN  -->  module pad  RX
//    ESP32 GPIO QCC_AT_RX_PIN  <--  module pad  TX
//
//  Protocol (see hardwhare/QCC5125/AT-commands.md): 115200 8N1, no flow
//  control. The vendor tools send the bare "AT+..." string with **no
//  CR/LF** - so this bridge appends nothing by default. A few commands
//  (AT+SMTIMEON/OFF) want \r\n: build with -D QCC_AT_CRLF=1 for those.
//  Valid commands are AT+GVER / AT+STATE / AT+IQ ... - "AT" alone or
//  "AT QA" are NOT valid and get no reply.
//
//  Pins/baud default to the Stamp-S3 free pins below; override from an
//  env's build_flags with  -D QCC_AT_RX_PIN=..  -D QCC_AT_TX_PIN=..
//  -D QCC_AT_BAUD=..  if a different board/wiring is used.
// ═════════════════════════════════════════════════════════════════════

#include <Arduino.h>

#ifndef QCC_AT_RX_PIN
#define QCC_AT_RX_PIN 39            // ESP32 RX  <-  module pad TX
#endif
#ifndef QCC_AT_TX_PIN
#define QCC_AT_TX_PIN 40            // ESP32 TX  ->  module pad RX
#endif
#ifndef QCC_AT_BAUD
#define QCC_AT_BAUD 115200
#endif
#ifndef QCC_AT_CRLF
#define QCC_AT_CRLF 0               // 1 = append \r\n (only AT+SMTIMEON/OFF need it)
#endif

namespace QccAtBridge {

inline void begin() {
  Serial1.begin(QCC_AT_BAUD, SERIAL_8N1, QCC_AT_RX_PIN, QCC_AT_TX_PIN);
  Serial.printf("[QCC-AT] bridge up: UART1 RX=GPIO%d TX=GPIO%d @ %d 8N1\n",
                (int) QCC_AT_RX_PIN, (int) QCC_AT_TX_PIN, (int) QCC_AT_BAUD);
}

// QCC5124 -> USB serial monitor. Call once per loop() tick.
inline void poll() {
  while (Serial1.available()) Serial.write(Serial1.read());
}

// USB serial monitor -> QCC5124. `line` is one already-trimmed input
// line. Returns true if it was an AT command (and has been forwarded),
// so the caller skips its own command dispatch for this line.
inline bool handleLine(const String &line) {
  if (line.length() < 2) return false;
  const char a = line[0], t = line[1];
  if ((a != 'A' && a != 'a') || (t != 'T' && t != 't')) return false;

  Serial1.print(line);             // bare string - vendor tools send no terminator
#if QCC_AT_CRLF
  Serial1.print("\r\n");
#endif
  Serial.printf("[QCC-AT] sent: %s\n", line.c_str());
  return true;
}

}  // namespace QccAtBridge
