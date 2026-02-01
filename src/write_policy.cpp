#include "write_policy.hpp"
#include "cache.hpp"
#include "system.hpp"

namespace sim {

void WriteBack::onWrite(Cache* cache, CacheLine* line, System* sys) {
    line->dirty = true;
}

void WriteThrough::onWrite(Cache* cache, CacheLine* line, System* sys) {
    line->dirty = false;
    // Notify next level about the write immediately.
    // We use PUTM as a simplified "write update" message for this model.
    sys->network.send(Message(MessageType::PUTM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
}

} // namespace sim
