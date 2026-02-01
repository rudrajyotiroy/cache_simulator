#pragma once
#include "message.hpp"

namespace sim {

class Cache;
class System;
struct CacheLine;

/**
 * @brief Strategy interface for handling cache writes (Write-Back vs Write-Through).
 */
class WritePolicy {
public:
    virtual ~WritePolicy() = default;

    /**
     * @brief Called whenever a write hits in a cache line.
     */
    virtual void onWrite(Cache* cache, CacheLine* line, System* sys) = 0;
};

/**
 * @brief Write-Back policy: sets the dirty bit and only updates higher levels on eviction.
 */
class WriteBack : public WritePolicy {
public:
    void onWrite(Cache* cache, CacheLine* line, System* sys) override;
};

/**
 * @brief Write-Through policy: immediately propagates the write to the next hierarchy level.
 */
class WriteThrough : public WritePolicy {
public:
    void onWrite(Cache* cache, CacheLine* line, System* sys) override;
};

} // namespace sim
