#pragma once

// Which Beolab 3500 hardware revision a piece of shared code (see
// MclBusWriter, MclData) applies to - confirmed identical wire
// protocol between the two (see MclBusReader.hpp), used only where a
// real difference has been confirmed (e.g. MclBusWriter's trailing
// pulse, MclData::buildSoundBits' gap-bit layout) instead of an
// unnamed bool flag.
enum class BL3500Version { MK1, MK2 };
