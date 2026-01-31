#include "write_policy.hpp"
#include "cache.hpp"
#include "system.hpp"

namespace sim {

void WriteBack::onWrite(Cache* cache, CacheLine* line, System* sys) {
    line->dirty = true;
}

void WriteThrough::onWrite(Cache* cache, CacheLine* line, System* sys) {
    line->dirty = false;
    sys->network.send(Message(MessageType::PUTM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
}

} // namespace sim
