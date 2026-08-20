# powerlink_mk2

Beolab 3500 Mk2's bus protocol - pure PowerLink, not the MCL/PL
"Datalink" protocol the Mk1 uses (see `../mcl_mk1/`). Not yet analyzed
or implemented.

Once the protocol is understood, mirror the `mcl_mk1/` structure here:

- `PlBusReader.hpp/.cpp` - bus capture and pulse-to-bit decoding
- `PlBusWriter.hpp/.cpp` - bit-to-pulse encoding and transmission
- `PlData.hpp/.cpp` - frame parsing/building

Entry point is `../main_mk2.cpp`; shared, protocol-agnostic GPIO
output code lives in `../common/`. No code should be shared between
this and `../mcl_mk1/` beyond `../common/` - the two protocols are
independent.
