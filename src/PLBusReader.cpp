#include "PLBusReader.hpp"

PLBusReader::PLBusReader(gpio_num_t pin) : _pin(pin) {}

bool IRAM_ATTR PLBusReader::onRxDone(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data) {
  BaseType_t high_task_wakeup = pdFALSE;
  QueueHandle_t queue = (QueueHandle_t) user_data;
  xQueueSendFromISR(queue, edata, &high_task_wakeup);
  return high_task_wakeup == pdTRUE;
}

uint8_t PLBusReader::classifyTcode(uint32_t us) {
  if (us < T1_US - T_STEP / 2 || us > T5_US + T_STEP / 2) return 0;
  uint32_t nearest = (us + T_STEP / 2) / T_STEP;
  if (nearest < 1 || nearest > 5) return 0;
  return (uint8_t) nearest;
}

uint32_t PLBusReader::periodUs(const rmt_symbol_word_t &sym) {
  return sym.duration0 + sym.duration1;
}

int PLBusReader::decodeBit(int lastBit, uint8_t tcode) {
  if (lastBit == 0) {
    if (tcode == 2) return 0;
    if (tcode == 3) return 1;
  } else {
    if (tcode == 1) return 0;
    if (tcode == 2) return 1;
  }
  return -1;
}

void PLBusReader::armReceive(int bufIdx) {
  ESP_ERROR_CHECK(rmt_receive(_rxChan, _rawSymbols[bufIdx], sizeof(_rawSymbols[bufIdx]), &_rxCfg));
}

void PLBusReader::begin() {
  pinMode(_pin, INPUT);

  _rxQueue = xQueueCreate(4, sizeof(rmt_rx_done_event_data_t));

  rmt_rx_channel_config_t rxChanCfg = {};
  rxChanCfg.gpio_num          = _pin;
  rxChanCfg.clk_src           = RMT_CLK_SRC_DEFAULT;
  rxChanCfg.resolution_hz     = 1000000u; // 1MHz -> 1us resolution
  rxChanCfg.mem_block_symbols = RAW_SYMBOLS_MAX;
  rxChanCfg.flags.invert_in   = false;
  rxChanCfg.flags.with_dma    = false;
  ESP_ERROR_CHECK(rmt_new_rx_channel(&rxChanCfg, &_rxChan));

  rmt_rx_event_callbacks_t cbs = {};
  cbs.on_recv_done = onRxDone;
  ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(_rxChan, &cbs, _rxQueue));

  _rxCfg.signal_range_min_ns = 2000;     // 2us  - suppress glitches
  _rxCfg.signal_range_max_ns = 20000000; // 20ms - marks end of a frame

  ESP_ERROR_CHECK(rmt_enable(_rxChan));
  armReceive(0);
}

bool PLBusReader::poll(String &bits) {
  if (_nextPending < _pendingFrames.size()) {
    bits = _pendingFrames[_nextPending++];
    return true;
  }
  _pendingFrames.clear();
  _nextPending = 0;

  rmt_rx_done_event_data_t rxData;
  if (xQueueReceive(_rxQueue, &rxData, portMAX_DELAY) != pdTRUE) return false;

  // re-arm into the other buffer immediately so we don't miss the
  // next Start pulse while this capture is being decoded
  _activeBuf = 1 - _activeBuf;
  armReceive(_activeBuf);
  decodeSymbols(rxData.received_symbols, rxData.num_symbols);

  if (_pendingFrames.empty()) return false;
  bits = _pendingFrames[_nextPending++];
  return true;
}

void PLBusReader::decodeSymbols(const rmt_symbol_word_t *sym, size_t n) {
  String bits;
  bool inFrame = false;
  int lastBit = 1; // Start symbol acts as an implicit "1" reference

  for (size_t i = 0; i < n; i++) {
    uint8_t tcode = classifyTcode(periodUs(sym[i]));

    if (tcode == 5) { // Start - a new frame begins (may interrupt a running one)
      if (inFrame && bits.length() > 0) _pendingFrames.push_back(bits);
      bits = "";
      inFrame = true;
      lastBit = 1;
      continue;
    }
    if (!inFrame) continue; // ignore noise before the first Start

    if (tcode == 4) { // Stop - frame ends
      _pendingFrames.push_back(bits);
      bits = "";
      inFrame = false;
      continue;
    }
    if (tcode == 1 || tcode == 2 || tcode == 3) {
      int bit = decodeBit(lastBit, tcode);
      if (bit < 0) {
        // still counts as a frame boundary (e.g. for TX-echo suppression
        // bookkeeping) even though the content is incomplete/invalid
        _pendingFrames.push_back(bits);
        inFrame = false;
        bits = "";
        continue;
      }
      bits += bit ? '1' : '0';
      lastBit = bit;
      continue;
    }
    _pendingFrames.push_back(bits); // same reasoning as the decode-error case above
    inFrame = false; // unclassified pulse width - drop the frame in progress
    bits = "";
  }
  if (inFrame && bits.length() > 0) _pendingFrames.push_back(bits);
}
