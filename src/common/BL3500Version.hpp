#pragma once

// Which Beolab 3500 hardware revision this build is running as -
// confirmed identical wire protocol between the two (see
// BusReader.hpp), differing only in frame content. main-standalone.cpp holds the
// one `blVersion` instance, auto-detected via GPIO in setup(), and
// uses it to pick between MclBusWriter/PlBusWriter and to gate MK1-
// vs MK2-only code paths. MclData's frame builders don't take this
// anymore - BusWriter's two subclasses call them with different
// arguments instead.

// Beolab 3500 mk.1 and LCS9000 speakers have serial numbers lower than 19343452 and have a socket marked "MCL", so you need to use a mk.1 cable for these speakers.  If the serial number of your speaker is 19343452 or above and the socket is marked "POWERLINK", you have the mk.2 speaker, so please use a mk.2 cable.  
enum class BL3500Version { MK1, MK2 };
