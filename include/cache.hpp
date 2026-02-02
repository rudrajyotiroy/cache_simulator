#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <functional>
#include <iostream>
#include <list>
#include "message.hpp"
#include "write_policy.hpp"
#include "cache_state.hpp"

namespace sim {

class System;

/**
 * @brief Represents a pending core request that is stalled due to a transient state.
 */
struct PendingRequest {
    CoreAccess type;
    std::function<void(std::string)> callback;
};

/**
 * @brief Represents a single cache line in the cache hierarchy.
 */
struct CacheLine {
    uint64_t addr = 0;       ///< The base address of the block.
    uint64_t tag = 0;        ///< The tag for identifying the block within a set.
    MSIState* state = nullptr; ///< Pointer to the current coherence state (State Pattern).
    bool dirty = false;      ///< Dirty bit for write-back.
    uint32_t way = 0;        ///< The way index within the set.
    int pending_acks = 0;    ///< Number of invalidation ACKs pending for this line.
    CoreAccess pending_access; ///< The access type that triggered a transient state.
    std::vector<PendingRequest> pending_requests; ///< Queue of stalled requests for this block.
};

/**
 * @brief Base Cache class. Subclassed to implement specific replacement policies.
 */
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
    std::unique_ptr<WritePolicy> write_policy;

    Cache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    virtual ~Cache() = default;

    uint64_t getSet(uint64_t addr) const { return (addr / block_size) % num_sets; }
    uint64_t getTag(uint64_t addr) const { return addr / (block_size * num_sets); }

    CacheLine* findLine(uint64_t addr);
    CacheLine* allocateLine(uint64_t addr, System* sys);

    MSIState* getState(uint64_t addr) {
        CacheLine* line = findLine(addr);
        return line ? line->state : getIState();
    }

    void handleMessage(const Message& msg, System* sys);
    void coreRead(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);
    void coreWrite(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);

    // Processes all stalled requests when a line reaches a stable state.
    void processPendingRequests(CacheLine* line, System* sys);

    virtual uint32_t findVictim(uint32_t set) = 0;
    virtual void updateOnAccess(uint32_t set, uint32_t way) = 0;
    virtual void updateOnInsert(uint32_t set, uint32_t way) = 0;

    static MSIState* getIState();
    static MSIState* getSState();
    static MSIState* getMState();
    static MSIState* getISDState();
    static MSIState* getIMADState();
    static MSIState* getIMAState();
    static MSIState* getMIAState();
    static MSIState* getSIAState();
};

class LRUCache : public Cache {
    std::vector<std::list<uint32_t>> lru_lists;
    std::vector<std::vector<std::list<uint32_t>::iterator>> iters;
public:
    LRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

class FIFOCache : public Cache {
    std::vector<std::list<uint32_t>> fifo_lists;
public:
    FIFOCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

class LFUCache : public Cache {
    std::vector<std::vector<uint32_t>> access_counts;
public:
    LFUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

class MRUCache : public Cache {
    std::vector<std::list<uint32_t>> mru_lists;
    std::vector<std::vector<std::list<uint32_t>::iterator>> iters;
public:
    MRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

class PLRUCache : public Cache {
    std::vector<std::vector<bool>> tree_bits;
    uint32_t num_levels;
public:
    PLRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

} // namespace sim
