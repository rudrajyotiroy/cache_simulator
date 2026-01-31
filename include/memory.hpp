#pragma once
#include <cstdint>

namespace sim {

class Memory {
public:
    uint64_t latency;
    Memory(uint64_t lat) : latency(lat) {}
};

} // namespace sim
