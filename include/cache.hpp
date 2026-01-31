#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <functional>
#include <iostream>
#include "replacement_policy.hpp"
#include "message.hpp"
#include "write_policy.hpp"
#include "cache_state.hpp"

namespace sim {

class System;

struct CacheLine {
    uint64_t addr = 0;
    uint64_t tag = 0;
    MSIState* state = nullptr;
    bool dirty = false;
    uint32_t way = 0;
    int pending_acks = 0;
    CoreAccess pending_access;
    std::function<void(std::string)> on_fill = nullptr; // Modified
};

class Cache {
public:
    uint32_t id;
    uint32_t size;
    uint32_t associativity;
    uint32_t block_size;
    uint32_t lookup_latency;
    uint32_t num_sets;
    int next_level_id = 999;
    std::string level_name;

    std::vector<std::vector<CacheLine>> sets;
    std::vector<std::unique_ptr<ReplacementPolicy>> repl_policies;
    std::unique_ptr<WritePolicy> write_policy;

    Cache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string repl_policy_type, std::string write_pol, std::string level_name);

    uint64_t getSet(uint64_t addr) { return (addr / block_size) % num_sets; }
    uint64_t getTag(uint64_t addr) { return addr / (block_size * num_sets); }

    CacheLine* findLine(uint64_t addr);
    CacheLine* allocateLine(uint64_t addr, System* sys);

    MSIState* getState(uint64_t addr) {
        CacheLine* line = findLine(addr);
        return line ? line->state : getIState();
    }

    void handleMessage(const Message& msg, System* sys);
    void coreRead(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);
    void coreWrite(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);

    static MSIState* getIState();
    static MSIState* getSState();
    static MSIState* getMState();
    static MSIState* getISDState();
    static MSIState* getIMADState();
    static MSIState* getIMAState();
    static MSIState* getMIAState();
    static MSIState* getSIAState();
};

} // namespace sim
