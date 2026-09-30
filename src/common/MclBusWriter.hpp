#pragma once

#include "BusWriter.hpp"

// MK1, speaking the B&O MCL/PL "Datalink" bus. Two real Master frame
// styles observed so far, both sniffed live via BeoPowerlinkDisplay
// 2026-09, selected via the constructor's MclMasterVariant:
// - MCL: an older real Master - no Sound(51) frame at all; volume and
//   channel/track are embedded directly in two 40-bit SelectSource(59)
//   frames (ValueType=72=VOLUME, then ValueType=64=channel/track). See
//   MclData::buildSelectSourceBits40().
// - PL: a Beocenter 2300 ("PL Master (BC2300)") - the originally-
//   confirmed behaviour: a Sound-setup (Value=90) frame + a 48-bit
//   SelectSource(59) frame (ValueType=96=activate, trailing
//   Byte6=0x00), sent twice.
// Only sendSource() branches on this - sendInit() ignores it for now
// (stays as before, not wired up to the variant yet). begin()/
// sendFrame()/pulse() are inherited as-is from BusWriter (not overridden
// - see BusWriter.hpp for why). sendVol() just forwards to BusWriter's
// "not available" default, since MK1 has no Vol feature. sendInit(value)
// is reachable through the debug "init <value>" Serial command (see
// SerialDebugCommands.cpp) to probe values other than the hardcoded one.
enum class MclMasterVariant { MCL, PL,EX };
static const char* const MclMasterVariant_NAMES[] = {"MCL", "PL", "EX"};


class MclBusWriter : public BusWriter {
public:
  explicit MclBusWriter(gpio_num_t pin, MclMasterVariant variant )
      : BusWriter(pin), _variant(variant) {}

  const char* name() const override { return MclMasterVariant_NAMES[(int)_variant]; }
  void sendSource(uint8_t device, uint8_t track) override;

  void sendVol(uint8_t value) override;
  void sendOff() override;
  void sendInit() override;
  void sendInit(uint8_t value) override;

private:
  void sendInitFrame(); // the Sound-setup frame only, no logging
  MclMasterVariant _variant;

  int counter = 0;
};
