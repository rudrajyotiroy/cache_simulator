#pragma once
#include <unordered_map>
#include <set>
#include <vector>
#include "message.hpp"

namespace sim {

class System; // Forward declaration

enum class DirState {
    I, S, M,
    IS_D, IM_AD, IM_A,
    S_INV,
    M_INV,
};

struct DirEntry {
    DirState state = DirState::I;
    std::set<int> sharers;
    int owner = -1;
    int pending_acks = 0;
    int requestor = -1;
};

class Directory {
public:
    std::unordered_map<uint64_t, DirEntry> table;

    DirEntry& getEntry(uint64_t addr) {
        return table[addr];
    }

    void handleMessage(const Message& msg, System* sys);
};

} // namespace sim
