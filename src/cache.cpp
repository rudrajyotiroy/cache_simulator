#include "cache.hpp"
#include "cache_state.hpp"
#include "system.hpp"
#include "write_policy.hpp"

namespace sim {

Cache::Cache(uint32_t id, uint32_t size, uint32_t assoc, uint32_t block_sz, uint32_t lat, std::string repl_policy_type, std::string write_pol, std::string level_name)
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
        if (repl_policy_type == "LRU") repl_policies.push_back(std::make_unique<LRUPolicy>(assoc));
        else if (repl_policy_type == "LFU") repl_policies.push_back(std::make_unique<LFUPolicy>(assoc));
        else if (repl_policy_type == "FIFO") repl_policies.push_back(std::make_unique<FIFOPolicy>(assoc));
        else if (repl_policy_type == "PLRU") repl_policies.push_back(std::make_unique<PLRUPolicy>(assoc));
        else if (repl_policy_type == "MRU") repl_policies.push_back(std::make_unique<MRUPolicy>(assoc));
        else repl_policies.push_back(std::make_unique<LRUPolicy>(assoc));
    }

    if (write_pol == "WriteThrough") write_policy = std::make_unique<WriteThrough>();
    else write_policy = std::make_unique<WriteBack>();
}

CacheLine* Cache::findLine(uint64_t addr) {
    uint64_t set = getSet(addr);
    uint64_t tag = getTag(addr);
    for (auto& line : sets[set]) {
        if (line.tag == tag && line.state != getIState()) return &line;
    }
    return nullptr;
}

CacheLine* Cache::allocateLine(uint64_t addr, System* sys) {
    uint64_t set = getSet(addr);
    uint32_t victim_way = repl_policies[set]->victim();
    CacheLine& line = sets[set][victim_way];

    if (line.state != getIState()) {
        line.state->onReplacement(this, &line, sys);
    }

    line.addr = (addr / block_size) * block_size;
    line.tag = getTag(addr);
    line.state = getIState();
    line.dirty = false;
    line.pending_acks = 0;
    line.on_fill = nullptr;
    repl_policies[set]->update_on_insert(victim_way);
    return &line;
}

void Cache::coreRead(uint64_t addr, System* sys, std::function<void(std::string)> on_complete) {
    CacheLine* line = findLine(addr);
    if (!line) {
        line = allocateLine(addr, sys);
    }
    line->on_fill = on_complete;
    line->state->onRead(this, line, sys);
    repl_policies[getSet(addr)]->access(line->way);
}

void Cache::coreWrite(uint64_t addr, System* sys, std::function<void(std::string)> on_complete) {
    CacheLine* line = findLine(addr);
    if (!line) {
        line = allocateLine(addr, sys);
    }
    line->on_fill = on_complete;
    line->state->onWrite(this, line, sys);
    repl_policies[getSet(addr)]->access(line->way);
}

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
        case MessageType::GETS:
        case MessageType::GETM:
            if (line && (line->state == getSState() || line->state == getMState())) {
                Message resp(MessageType::DATA, id, msg.sender_id, msg.address);
                resp.resolved_by = level_name;
                sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
            } else {
                sys->network.send(Message(msg.type, msg.sender_id, next_level_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
            }
            break;
        case MessageType::DATA: line->state->onData(this, line, sys, msg); break;
        case MessageType::ACK: line->state->onAck(this, line, sys, msg); break;
        case MessageType::INV: line->state->onInv(this, line, sys, msg); break;
        case MessageType::FWD_GETS: line->state->onFwdGets(this, line, sys, msg); break;
        case MessageType::FWD_GETM: line->state->onFwdGetm(this, line, sys, msg); break;
        case MessageType::PUT_ACK: line->state->onPutAck(this, line, sys, msg); break;
        default: break;
    }
}

} // namespace sim
