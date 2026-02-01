#pragma once
#include <cstdint>

namespace sim {

/**
 * @brief Represents the main memory of the system.
 */
class Memory {
public:
    uint64_t latency; ///< Parameterized memory access latency.

    Memory(uint64_t lat) : latency(lat) {}
};

} // namespace sim
