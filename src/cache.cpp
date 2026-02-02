#include "cache.hpp"
#include "cache_state.hpp"
#include "system.hpp"
#include "write_policy.hpp"
#include <algorithm>

namespace sim {

/**
 * @brief Base Cache constructor.
 */
Cache::Cache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : id(id), size(size), associativity(assoc), block_size(block_sz), lookup_latency(lat), level_name(level_name) {
    num_sets = size / (assoc * block_sz);
    if (num_sets == 0) num_sets = 1;
    sets.resize(num_sets);
    for (uint32_t i = 0; i < num_sets; ++i) {
        sets[i].resize(assoc);
        for(uint32_t j=0; j<assoc; ++j) {
            sets[i][j].way = j;
            sets[i][j].state = getIState();
        }
    }
    if (write_pol == "WriteThrough") write_policy = std::make_unique<WriteThrough>();
    else write_policy = std::make_unique<WriteBack>();
}

/**
 * @brief Performs a tag lookup.
 */
CacheLine* Cache::findLine(uint64_t addr) {
    uint64_t set = getSet(addr);
    uint64_t tag = getTag(addr);
    for (auto& line : sets[set]) {
        if (line.tag == tag && line.state != getIState()) return &line;
    }
    return nullptr;
}

/**
 * @brief Allocates a new line and handles eviction.
 */
CacheLine* Cache::allocateLine(uint64_t addr, System* sys) {
    uint64_t set = getSet(addr);
    uint32_t victim_way = findVictim(set);
    CacheLine& line = sets[set][victim_way];

    // Evict if necessary
    if (line.state != getIState()) {
        line.state->onReplacement(this, &line, sys);
    }

    line.addr = (addr / block_size) * block_size;
    line.tag = getTag(addr);
    line.state = getIState();
    line.dirty = false;
    line.pending_acks = 0;
    line.pending_requests.clear();

    updateOnInsert(set, victim_way);
    return &line;
}

/**
 * @brief Handles a core read request. Stalls if in transient state.
 */
void Cache::coreRead(uint64_t addr, System* sys, std::function<void(std::string)> on_complete) {
    CacheLine* line = findLine(addr);
    if (!line) {
        line = allocateLine(addr, sys);
    }

    if (on_complete) {
        line->pending_requests.push_back({CoreAccess::READ, on_complete});
    }

    // Delegate to state. Stable states will process the request. Transient states will return.
    line->state->onRead(this, line, sys);
    updateOnAccess(getSet(addr), line->way);
}

/**
 * @brief Handles a core write request. Stalls if in transient state.
 */
void Cache::coreWrite(uint64_t addr, System* sys, std::function<void(std::string)> on_complete) {
    CacheLine* line = findLine(addr);
    if (!line) {
        line = allocateLine(addr, sys);
    }

    if (on_complete) {
        line->pending_requests.push_back({CoreAccess::WRITE, on_complete});
    }

    line->state->onWrite(this, line, sys);
    updateOnAccess(getSet(addr), line->way);
}

/**
 * @brief Processes queued requests for a line after it stabilizes.
 */
void Cache::processPendingRequests(CacheLine* line, System* sys) {
    while (!line->pending_requests.empty()) {
        auto req = line->pending_requests.front();

        // If state is still transient (e.g. upgrade needed), stop and wait.
        if (req.type == CoreAccess::READ) {
            if (line->state == getSState() || line->state == getMState()) {
                line->pending_requests.erase(line->pending_requests.begin());
                sys->scheduler.schedule(sys->scheduler.getTime() + lookup_latency, [req, this](){
                    req.callback(level_name);
                });
            } else {
                line->state->onRead(this, line, sys);
                break;
            }
        } else {
            if (line->state == getMState()) {
                line->pending_requests.erase(line->pending_requests.begin());
                write_policy->onWrite(this, line, sys);
                sys->scheduler.schedule(sys->scheduler.getTime() + lookup_latency, [req, this](){
                    req.callback(level_name);
                });
            } else {
                line->state->onWrite(this, line, sys);
                break;
            }
        }
    }
}

/**
 * @brief Handles incoming coherence messages.
 */
void Cache::handleMessage(const Message& msg, System* sys) {
    CacheLine* line = findLine(msg.address);
    if (!line) {
        if (msg.type == MessageType::GETS || msg.type == MessageType::GETM ||
            msg.type == MessageType::PUTS || msg.type == MessageType::PUTM) {
            sys->network.send(Message(msg.type, msg.sender_id, next_level_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
        }
        return;
    }

    switch (msg.type) {
        case MessageType::DATA: line->state->onData(this, line, sys, msg); break;
        case MessageType::ACK: line->state->onAck(this, line, sys, msg); break;
        case MessageType::INV: line->state->onInv(this, line, sys, msg); break;
        case MessageType::FWD_GETS: line->state->onFwdGets(this, line, sys, msg); break;
        case MessageType::FWD_GETM: line->state->onFwdGetm(this, line, sys, msg); break;
        case MessageType::PUT_ACK: line->state->onPutAck(this, line, sys, msg); break;
        default: break;
    }
}

// --- LRUCache ---
LRUCache::LRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : Cache(id, size, assoc, block_sz, lat, write_pol, level_name) {
    lru_lists.resize(num_sets); iters.resize(num_sets);
    for(uint32_t i=0; i<num_sets; ++i) {
        iters[i].resize(assoc);
        for(uint32_t j=0; j<assoc; ++j) {
            lru_lists[i].push_back(j); iters[i][j] = std::prev(lru_lists[i].end());
        }
    }
}
uint32_t LRUCache::findVictim(uint32_t set) { return lru_lists[set].back(); }
void LRUCache::updateOnAccess(uint32_t set, uint32_t way) {
    lru_lists[set].erase(iters[set][way]); lru_lists[set].push_front(way); iters[set][way] = lru_lists[set].begin();
}
void LRUCache::updateOnInsert(uint32_t set, uint32_t way) { updateOnAccess(set, way); }

// --- FIFOCache ---
FIFOCache::FIFOCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : Cache(id, size, assoc, block_sz, lat, write_pol, level_name) {
    fifo_lists.resize(num_sets);
    for(uint32_t i=0; i<num_sets; ++i) for(uint32_t j=0; j<assoc; ++j) fifo_lists[i].push_back(j);
}
uint32_t FIFOCache::findVictim(uint32_t set) { return fifo_lists[set].back(); }
void FIFOCache::updateOnAccess(uint32_t set, uint32_t way) {}
void FIFOCache::updateOnInsert(uint32_t set, uint32_t way) {
    auto it = std::find(fifo_lists[set].begin(), fifo_lists[set].end(), way);
    if (it != fifo_lists[set].end()) fifo_lists[set].erase(it);
    fifo_lists[set].push_front(way);
}

// --- LFUCache ---
LFUCache::LFUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : Cache(id, size, assoc, block_sz, lat, write_pol, level_name) {
    access_counts.assign(num_sets, std::vector<uint32_t>(assoc, 0));
}
uint32_t LFUCache::findVictim(uint32_t set) {
    return std::distance(access_counts[set].begin(), std::min_element(access_counts[set].begin(), access_counts[set].end()));
}
void LFUCache::updateOnAccess(uint32_t set, uint32_t way) { access_counts[set][way]++; }
void LFUCache::updateOnInsert(uint32_t set, uint32_t way) { access_counts[set][way] = 1; }

// --- MRUCache ---
MRUCache::MRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : Cache(id, size, assoc, block_sz, lat, write_pol, level_name) {
    mru_lists.resize(num_sets); iters.resize(num_sets);
    for(uint32_t i=0; i<num_sets; ++i) {
        iters[i].resize(assoc);
        for(uint32_t j=0; j<assoc; ++j) {
            mru_lists[i].push_back(j); iters[i][j] = std::prev(mru_lists[i].end());
        }
    }
}
uint32_t MRUCache::findVictim(uint32_t set) { return mru_lists[set].front(); }
void MRUCache::updateOnAccess(uint32_t set, uint32_t way) {
    mru_lists[set].erase(iters[set][way]); mru_lists[set].push_front(way); iters[set][way] = mru_lists[set].begin();
}
void MRUCache::updateOnInsert(uint32_t set, uint32_t way) { updateOnAccess(set, way); }

// --- PLRUCache ---
PLRUCache::PLRUCache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string write_pol, std::string level_name)
    : Cache(id, size, assoc, block_sz, lat, write_pol, level_name) {
    tree_bits.assign(num_sets, std::vector<bool>(assoc - 1, false));
    num_levels = 0; uint32_t temp = assoc; while (temp > 1) { temp >>= 1; num_levels++; }
}
uint32_t PLRUCache::findVictim(uint32_t set) {
    uint32_t curr = 0; uint32_t res = 0;
    for (uint32_t i = 0; i < num_levels; ++i) {
        if (tree_bits[set][curr]) { res |= (1u << (num_levels - 1 - i)); curr = 2 * curr + 2; }
        else { curr = 2 * curr + 1; }
    }
    return res;
}
void PLRUCache::updateOnAccess(uint32_t set, uint32_t way) {
    uint32_t curr = 0;
    for (uint32_t i = 0; i < num_levels; ++i) {
        bool bit = (way >> (num_levels - 1 - i)) & 1; tree_bits[set][curr] = !bit; curr = 2 * curr + (bit ? 2 : 1);
    }
}
void PLRUCache::updateOnInsert(uint32_t set, uint32_t way) { updateOnAccess(set, way); }

} // namespace sim
