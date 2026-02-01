#include "directory.hpp"
#include "system.hpp"
#include <iostream>

namespace sim {

/**
 * @brief Handles coherence requests and maintains the global state of blocks.
 */
void Directory::handleMessage(const Message& msg, System* sys) {
    DirEntry& entry = getEntry(msg.address);

    switch (msg.type) {
        case MessageType::GETS:
            // Read request from a cache
            if (entry.state == DirState::I) {
                // Block not in any cache, fetch from memory
                entry.state = DirState::S;
                entry.sharers.insert(msg.sender_id);
                Message resp(MessageType::DATA, 999, msg.sender_id, msg.address);
                resp.resolved_by = "Memory";
                sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
            } else if (entry.state == DirState::S) {
                // Hit in Shared state
                entry.sharers.insert(msg.sender_id);
                Message resp(MessageType::DATA, 999, msg.sender_id, msg.address);
                resp.resolved_by = "Memory";
                sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
            } else if (entry.state == DirState::M) {
                // Block is modified in another cache, must downgrade it
                sys->network.send(Message(MessageType::FWD_GETS, msg.sender_id, entry.owner, msg.address), [sys](Message m){ sys->handleMessage(m); });
                entry.state = DirState::S;
                entry.sharers.insert(entry.owner);
                entry.sharers.insert(msg.sender_id);
                entry.owner = -1;
            }
            break;

        case MessageType::GETM:
            // Write request from a cache
            if (entry.state == DirState::I) {
                entry.state = DirState::M;
                entry.owner = msg.sender_id;
                Message resp(MessageType::DATA, 999, msg.sender_id, msg.address, 0);
                resp.resolved_by = "Memory";
                sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
            } else if (entry.state == DirState::S) {
                // Shared to Modified: Invalidate all other sharers
                int acks = 0;
                for (int sharer : entry.sharers) {
                    if (sharer != msg.sender_id) {
                        sys->network.send(Message(MessageType::INV, msg.sender_id, sharer, msg.address), [sys](Message m){ sys->handleMessage(m); });
                        acks++;
                    }
                }
                entry.state = DirState::M;
                entry.owner = msg.sender_id;
                entry.sharers.clear();
                Message resp(MessageType::DATA, 999, msg.sender_id, msg.address, acks);
                resp.resolved_by = "Memory";
                sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
            } else if (entry.state == DirState::M) {
                // Transfer ownership
                if (entry.owner != msg.sender_id) {
                    sys->network.send(Message(MessageType::FWD_GETM, msg.sender_id, entry.owner, msg.address), [sys](Message m){ sys->handleMessage(m); });
                    entry.owner = msg.sender_id;
                } else {
                    // Already the owner
                    Message resp(MessageType::DATA, 999, msg.sender_id, msg.address, 0);
                    resp.resolved_by = "Memory";
                    sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
                }
            }
            break;

        case MessageType::PUTS:
            // Clean eviction
            if (entry.state == DirState::S) {
                entry.sharers.erase(msg.sender_id);
                if (entry.sharers.empty()) entry.state = DirState::I;
                sys->network.send(Message(MessageType::PUT_ACK, 999, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
            }
            break;

        case MessageType::PUTM:
            // Dirty eviction (write-back)
            if (entry.state == DirState::M && entry.owner == msg.sender_id) {
                entry.state = DirState::I;
                entry.owner = -1;
                sys->network.send(Message(MessageType::PUT_ACK, 999, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
            }
            break;

        default: break;
    }
}

} // namespace sim
