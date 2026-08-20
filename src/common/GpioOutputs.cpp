#include "GpioOutputs.hpp"

namespace GpioOutputs {

const SourcePin SOURCE_PINS[] = {
  {192, GPIO_NUM_4},  // TV
  {193, GPIO_NUM_5},  // Radio
 // {194, GPIO_NUM_12}, // V.Aux
 // {195, GPIO_NUM_13}, // A.Aux
 // {197, GPIO_NUM_14}, // V.Tape
 // {198, GPIO_NUM_15}, // DVD
 // {202, GPIO_NUM_16}, // Sat
  {203, GPIO_NUM_17}, // PC
 // {209, GPIO_NUM_18}, // A.Tape
  {210, GPIO_NUM_19}, // CD
  {211, GPIO_NUM_22}, // Phono
 // {212, GPIO_NUM_23}, // A.Tape2
 // {215, GPIO_NUM_27}, // CD2
};
const size_t SOURCE_PIN_COUNT = sizeof(SOURCE_PINS) / sizeof(SOURCE_PINS[0]);

void beginSourcePins() {
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    pinMode(SOURCE_PINS[i].pin, OUTPUT);
    digitalWrite(SOURCE_PINS[i].pin, LOW);
  }
}

void setActiveSourcePin(int device) {
  for (size_t i = 0; i < SOURCE_PIN_COUNT; i++) {
    digitalWrite(SOURCE_PINS[i].pin, SOURCE_PINS[i].device == device ? HIGH : LOW);
  }
}

const gpio_num_t KEY_PINS[] = {KEY_PIN_LEFT, KEY_PIN_RIGHT, KEY_PIN_STOP};
const size_t KEY_PIN_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

void beginKeyPins() {
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    pinMode(KEY_PINS[i], INPUT); // idle floating (open switch)
  }
}

void pressKey(gpio_num_t pin) {
  for (size_t i = 0; i < KEY_PIN_COUNT; i++) {
    if (KEY_PINS[i] != pin) pinMode(KEY_PINS[i], INPUT);
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(100);
  pinMode(pin, INPUT);
}

}
