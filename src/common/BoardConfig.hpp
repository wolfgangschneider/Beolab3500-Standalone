#pragma once

#include <Arduino.h>
#include "driver/gpio.h"

// ═════════════════════════════════════════════════════════════════════
//  Board configuration - the ONE place every board-specific value lives.
//
//  Select a board with  -D BOARD_<x>  in the env's build_flags
//  (platformio.ini). Every board below lists the FULL set of values -
//  nothing is shared between them, so they can diverge freely.
//
//  MK1 vs MK2 is NOT a board choice: one firmware, auto-detected at
//  runtime via MK2_DETECTED_PIN (see main-standalone.cpp setup()).
// ═════════════════════════════════════════════════════════════════════

// device id (192 = TV, 193 = Radio, ... see MclData) -> one GPIO driven
// HIGH while that source is active. Consumed by GpioOutputs.
struct SourcePin { int device; gpio_num_t pin; };


// ─────────────────────────────────────────────────────────────────────
#if defined(BOARD_WROOVER)
// classic ESP32  -  esp32_wrover dev board
// ─────────────────────────────────────────────────────────────────────
constexpr const char *BOARD_NAME = "wrover";

constexpr gpio_num_t MCL_RX_PIN       = GPIO_NUM_34;   // input-only pin - fine, read-only
constexpr gpio_num_t MCL_TX_PIN       = GPIO_NUM_25;
constexpr gpio_num_t MK2_MUTE_PIN     = GPIO_NUM_26;
constexpr gpio_num_t MK2_BL_MUTE_PIN  = GPIO_NUM_33;
constexpr gpio_num_t MK2_DETECTED_PIN = GPIO_NUM_32;

constexpr gpio_num_t KEY_PIN_LEFT     = GPIO_NUM_5;
constexpr gpio_num_t KEY_PIN_RIGHT    = GPIO_NUM_18;
constexpr gpio_num_t KEY_PIN_STOP     = GPIO_NUM_23;
// nav keys MUST stay off GPIO6-11 (internal SPI-flash bus: CLK/SD0..3/
// CMD). Reconfiguring those as plain GPIO glitches the flash bus the
// CPU fetches code from -> bootloop with no panic dump.

constexpr size_t RMT_RX_MEM_SYMBOLS   = 512;           // 8 RMT channels share 512 words

// all disabled for now. If A.Tape (209) / A.Tape2 (212) ever get
// enabled here, first move KEY_PIN_RIGHT / KEY_PIN_STOP off 18 / 23.
inline constexpr SourcePin SOURCE_PINS[] = {
// {192, GPIO_NUM_33},   // TV
// {193, GPIO_NUM_34},   // Radio   (input-only pin - fix before use)
// {194, GPIO_NUM_12},   // V.Aux
// {195, GPIO_NUM_13},   // A.Aux
// {197, GPIO_NUM_14},   // V.Tape
// {198, GPIO_NUM_15},   // DVD
// {202, GPIO_NUM_16},   // Sat
// {203, GPIO_NUM_17},   // PC
// {209, GPIO_NUM_18},   // A.Tape
// {210, GPIO_NUM_19},   // CD
// {211, GPIO_NUM_22},   // Phono
// {212, GPIO_NUM_23},   // A.Tape2
// {215, GPIO_NUM_27},   // CD2
};


// ─────────────────────────────────────────────────────────────────────
#elif defined(BOARD_M5STAMP_S3)
// M5Stack Stamp S3  (ESP32-S3)
// ─────────────────────────────────────────────────────────────────────
constexpr const char *BOARD_NAME = "Stamp-S3";

constexpr gpio_num_t MCL_RX_PIN       = GPIO_NUM_1;
constexpr gpio_num_t MCL_TX_PIN       = GPIO_NUM_3;
constexpr gpio_num_t MK2_MUTE_PIN     = GPIO_NUM_5;
constexpr gpio_num_t MK2_BL_MUTE_PIN  = GPIO_NUM_9;
constexpr gpio_num_t MK2_DETECTED_PIN = GPIO_NUM_43;

constexpr gpio_num_t KEY_PIN_LEFT     = GPIO_NUM_5;
constexpr gpio_num_t KEY_PIN_RIGHT    = GPIO_NUM_7;
constexpr gpio_num_t KEY_PIN_STOP     = GPIO_NUM_9;
// S3 flash pinout differs from classic ESP32 - GPIO7/9 are free here.

constexpr size_t RMT_RX_MEM_SYMBOLS   = 64;            // S3: only 4 RMT channels, little memory

// placeholders - final assignment TBD, can change version to version.
// Only G0-15 and G39-46 are broken out on the Stamp S3.
inline constexpr SourcePin SOURCE_PINS[] = {
  {192, GPIO_NUM_44},   // TV
  {193, GPIO_NUM_43},   // Radio
// {194, GPIO_NUM_9},    // V.Aux
// {195, GPIO_NUM_10},   // A.Aux
// {197, GPIO_NUM_11},   // V.Tape
// {198, GPIO_NUM_15},   // DVD
// {202, GPIO_NUM_39},   // Sat
// {203, GPIO_NUM_7},    // PC
// {209, GPIO_NUM_40},   // A.Tape
// {210, GPIO_NUM_14},   // CD
// {211, GPIO_NUM_9},    // Phono
// {212, GPIO_NUM_41},   // A.Tape2
// {215, GPIO_NUM_42},   // CD2
};


// ─────────────────────────────────────────────────────────────────────
#elif defined(BOARD_S3_MINI)
// ESP32-S3-MINI-1 module  (ESP32-S3)
// should be pin compatible to M5-Stack Stamp S3
// ─────────────────────────────────────────────────────────────────────
constexpr const char *BOARD_NAME = "S3-MINI";

constexpr gpio_num_t MCL_RX_PIN       = GPIO_NUM_8;
constexpr gpio_num_t MCL_TX_PIN       = GPIO_NUM_9;
constexpr gpio_num_t MK2_MUTE_PIN     = GPIO_NUM_10;
constexpr gpio_num_t MK2_BL_MUTE_PIN  = GPIO_NUM_12;
constexpr gpio_num_t MK2_DETECTED_PIN = GPIO_NUM_3;//GPIO_NUM_43;

constexpr gpio_num_t KEY_PIN_LEFT     = GPIO_NUM_10;
constexpr gpio_num_t KEY_PIN_RIGHT    = GPIO_NUM_11;
constexpr gpio_num_t KEY_PIN_STOP     = GPIO_NUM_12;
// S3 flash pinout differs from classic ESP32 - GPIO7/9 are free here.

constexpr size_t RMT_RX_MEM_SYMBOLS   = 64;            // S3: only 4 RMT channels, little memory

// placeholders - final assignment TBD, can change version to version.
inline constexpr SourcePin SOURCE_PINS[] = {
  {192, GPIO_NUM_2},   // TV
  {193, GPIO_NUM_3},   // Radio
// {194, GPIO_NUM_9},    // V.Aux
// {195, GPIO_NUM_10},   // A.Aux
// {197, GPIO_NUM_11},   // V.Tape
// {198, GPIO_NUM_15},   // DVD
// {202, GPIO_NUM_39},   // Sat
// {203, GPIO_NUM_7},    // PC
// {209, GPIO_NUM_40},   // A.Tape
// {210, GPIO_NUM_14},   // CD
// {211, GPIO_NUM_9},    // Phono
// {212, GPIO_NUM_41},   // A.Tape2
// {215, GPIO_NUM_42},   // CD2
};


// ─────────────────────────────────────────────────────────────────────
#else
  #error "No board selected - add  -D BOARD_WROOVER / BOARD_M5STAMP_S3 / BOARD_S3_MINI  to the env's build_flags in platformio.ini"
#endif
// ─────────────────────────────────────────────────────────────────────


inline constexpr size_t SOURCE_PIN_COUNT = sizeof(SOURCE_PINS) / sizeof(SOURCE_PINS[0]);
