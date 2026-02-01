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
    std::function<void(std::string)> on_fill = nullptr; ///< Callback to execute when data is filled.
};

/**
 * @brief Base Cache class. Subclassed to implement specific replacement policies.
 *
 * This class follows a "Maximally OOP" design where replacement policies are subclasses
 * of the Cache class itself, using polymorphism to determine victims and update states.
 */
class Cache {
public:
    uint32_t id;             ///< Unique identifier for the cache instance.
    uint32_t size;           ///< Total size of the cache in bytes.
    uint32_t associativity;  ///< Number of lines per set.
    uint32_t block_size;     ///< Size of each cache block in bytes.
    uint32_t lookup_latency; ///< Time in cycles to perform a tag lookup.
    uint32_t num_sets;       ///< Calculated number of sets.
    int next_level_id = 999; ///< ID of the next component in hierarchy (e.g., L2, L3, or Directory).
    std::string level_name;  ///< Human-readable name for tracing (e.g., "L1_0").

    std::vector<std::vector<CacheLine>> sets;      ///< The actual storage for cache lines.
    std::unique_ptr<WritePolicy> write_policy;    ///< Strategy for handling writes (WB/WT).

    /**
     * @brief Construct a new Cache object.
     */
    Cache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);

    virtual ~Cache() = default;

    // Address decoding helpers
    uint64_t getSet(uint64_t addr) const { return (addr / block_size) % num_sets; }
    uint64_t getTag(uint64_t addr) const { return addr / (block_size * num_sets); }

    /**
     * @brief Looks up a line in the cache. Returns nullptr if miss.
     */
    CacheLine* findLine(uint64_t addr);

    /**
     * @brief Allocates a new line, potentially triggering an eviction via findVictim().
     */
    CacheLine* allocateLine(uint64_t addr, System* sys);

    /**
     * @brief Returns the current MSI state of a block.
     */
    MSIState* getState(uint64_t addr) {
        CacheLine* line = findLine(addr);
        return line ? line->state : getIState();
    }

    /**
     * @brief Handles incoming coherence messages.
     */
    void handleMessage(const Message& msg, System* sys);

    /**
     * @brief Interface for core-initiated read requests.
     */
    void coreRead(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);

    /**
     * @brief Interface for core-initiated write requests.
     */
    void coreWrite(uint64_t addr, System* sys, std::function<void(std::string)> on_complete = nullptr);

    // Polymorphic Replacement Policy Methods
    virtual uint32_t findVictim(uint32_t set) = 0;
    virtual void updateOnAccess(uint32_t set, uint32_t way) = 0;
    virtual void updateOnInsert(uint32_t set, uint32_t way) = 0;

    // Static accessors for MSI state singletons
    static MSIState* getIState();
    static MSIState* getSState();
    static MSIState* getMState();
    static MSIState* getISDState();
    static MSIState* getIMADState();
    static MSIState* getIMAState();
    static MSIState* getMIAState();
    static MSIState* getSIAState();
};

/**
 * @brief Least Recently Used (LRU) Cache subclass.
 */
class LRUCache : public Cache {
    std::vector<std::list<uint32_t>> lru_lists;
    std::vector<std::vector<std::list<uint32_t>::iterator>> iters;
public:
    LRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

/**
 * @brief First-In-First-Out (FIFO) Cache subclass.
 */
class FIFOCache : public Cache {
    std::vector<std::list<uint32_t>> fifo_lists;
public:
    FIFOCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

/**
 * @brief Least Frequently Used (LFU) Cache subclass.
 */
class LFUCache : public Cache {
    std::vector<std::vector<uint32_t>> access_counts;
public:
    LFUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

/**
 * @brief Most Recently Used (MRU) Cache subclass.
 */
class MRUCache : public Cache {
    std::vector<std::list<uint32_t>> mru_lists;
    std::vector<std::vector<std::list<uint32_t>::iterator>> iters;
public:
    MRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name);
    uint32_t findVictim(uint32_t set) override;
    void updateOnAccess(uint32_t set, uint32_t way) override;
    void updateOnInsert(uint32_t set, uint32_t way) override;
};

/**
 * @brief Pseudo-LRU (Tree-based) Cache subclass.
 */
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
