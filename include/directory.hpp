#pragma once
#include <unordered_map>
#include <set>
#include <vector>
#include "message.hpp"

namespace sim {

class System;

/**
 * @brief States for the directory entries.
 */
enum class DirState {
    I, ///< Invalid (not in any cache)
    S, ///< Shared (in one or more caches, consistent with memory)
    M  ///< Modified (in exactly one cache, potentially dirty)
};

/**
 * @brief Entry in the central directory for a specific memory block.
 */
struct DirEntry {
    DirState state = DirState::I;
    std::set<int> sharers; ///< Set of Cache IDs that have a shared copy.
    int owner = -1;        ///< Cache ID of the owner (for M state).
};

/**
 * @brief Implements a centralized directory for cache coherence.
 */
class Directory {
public:
    std::unordered_map<uint64_t, DirEntry> table;

    /**
     * @brief Returns the entry for a given address. Creates one if it doesn't exist.
     */
    DirEntry& getEntry(uint64_t addr) {
        return table[addr];
    }

    /**
     * @brief Processes coherence messages from caches.
     */
    void handleMessage(const Message& msg, System* sys);
};

} // namespace sim
