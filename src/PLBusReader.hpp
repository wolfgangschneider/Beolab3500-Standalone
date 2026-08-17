#pragma once

#include <Arduino.h>
#include <vector>
#include "driver/rmt_rx.h"

// Reads and decodes the B&O MCL/PL "Datalink" bus via the ESP32 RMT
// peripheral. Per B&O MCL-2 Service Manual ("Datalink '86"):
// - Bus is wired-OR, active-low, idle=HIGH via pull-up. The
//   transmitting unit pulls the line LOW for a fixed-width strobe;
//   the timing symbol (t1..t5) is the FULL period (LOW+HIGH) between
//   two successive falling edges, not the LOW width alone.
// - Timing symbols: t1=3.125ms t2=6.250ms t3=9.375ms (data),
//   t4=12.500ms (Stop), t5=15.625ms (Start).
// - Differential bit code: new bit depends on both the timing symbol
//   and the previously decoded bit (last=0: t2->0, t3->1; last=1:
//   t1->0, t2->1).
// - Frame = Start | Format(3) | Address(to)(5) | Address(from)(4) |
//   Data | Stop (manual fig. 2045-4).
class PLBusReader {
public:
  explicit PLBusReader(gpio_num_t pin);

  // configures the RMT RX channel and starts listening
  void begin();

  // blocks until the next complete frame is available, fills `bits`
  // and returns true. A single RMT capture can contain more than one
  // frame (e.g. our own two-frame TX echo) - those are queued and
  // returned on subsequent calls without waiting for a new capture.
  bool poll(String &bits);

private:
  static constexpr size_t RAW_SYMBOLS_MAX = 512;

  static constexpr uint32_t T1_US = 3125;
  static constexpr uint32_t T2_US = 6250;
  static constexpr uint32_t T3_US = 9375;
  static constexpr uint32_t T4_US = 12500; // Stop
  static constexpr uint32_t T5_US = 15625; // Start
  static constexpr uint32_t T_STEP = 3125;

  gpio_num_t _pin;

  rmt_channel_handle_t _rxChan = nullptr;
  QueueHandle_t        _rxQueue = nullptr;
  // double-buffered so the next capture can start immediately (in the
  // other buffer) while this one is still being decoded - otherwise
  // the gap between "capture done" and "re-armed" drops the next
  // Start pulse
  rmt_symbol_word_t    _rawSymbols[2][RAW_SYMBOLS_MAX];
  rmt_receive_config_t _rxCfg = {};
  int                  _activeBuf = 0;

  // frames decoded from the most recent capture, not yet returned by poll()
  std::vector<String> _pendingFrames;
  size_t               _nextPending = 0;

  void armReceive(int bufIdx);
  void decodeSymbols(const rmt_symbol_word_t *sym, size_t n); // fills _pendingFrames

  // classifies a pulse width to the nearest timing symbol (1..5), 0 = out of range
  static uint8_t classifyTcode(uint32_t us);
  static uint32_t periodUs(const rmt_symbol_word_t &sym);
  // differential bit decode. Returns -1 on protocol error.
  static int decodeBit(int lastBit, uint8_t tcode);

  static bool IRAM_ATTR onRxDone(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data);
};
