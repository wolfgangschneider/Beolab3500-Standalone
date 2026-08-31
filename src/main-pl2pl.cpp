/*
  Beolab3500-PL2PL

  Minimal trigger board for a Beolab 3500 MKII on the B&O PL bus:
  hold GPIO5 HIGH and it sends the captured MKII power-on/init frame
  once on GPIO1 - nothing else. No RX, no MK1 support, no other
  commands. Talks to a plain `BusWriter` for the raw wire framing
  (begin/sendFrame/pulse) - not PlBusWriter/MclData, those aren't
  needed here, this never does source selection or volume.
  BusWriter's sendSource()/sendVol()/sendInit() defaults are harmless
  no-ops (not pure virtual), so a bare BusWriter is a valid,
  concrete, instantiable type here - see common/BusWriter.hpp.
  ESP32 only (see platformio.ini's pl2pl_m5_stamp_S3 env) - dropped
  ATtiny85/Digispark support, which this used to also target, because
  common/ needs gpio_num_t and doesn't build on AVR.

  Per B&O MCL-2 Service Manual ("Datalink '86"): MKII traffic has one
  extra trailing t1 pulse after Stop, on top of what sendFrame() itself
  already sends (see BusWriter::sendFrame()).
*/

#include <Arduino.h>
#include "common/BusWriter.hpp"

constexpr gpio_num_t TX_PIN      = GPIO_NUM_3;
constexpr gpio_num_t TRIGGER_PIN = GPIO_NUM_5;

// Captured MKII power-on frame (Command=49, unrecognized/literal -
// same frame PlBusWriter::sendInit() sends, see there for more
// context/history).
constexpr const char *INIT_BITS = "0011000111100111111100000000100";

static BusWriter bus(TX_PIN);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Beolab3500-PL2PL");

  bus.begin();
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
}

void loop() {
  static bool wasHigh = false;
  bool nowHigh = digitalRead(TRIGGER_PIN) == HIGH;

  if (nowHigh && !wasHigh) {
    delay(1000); // wait if datapin is connected to set the source and display is on or use a jumper
    Serial.println("-> sending init");
    bus.sendFrame(INIT_BITS);
    bus.pulse(1); // MKII trailing pulse
  }
  wasHigh = nowHigh;
}
