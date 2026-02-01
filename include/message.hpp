#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <functional>
#include "scheduler.hpp"

namespace sim {

/**
 * @brief Types of coherence and data messages.
 */
enum class MessageType {
    GETS,    ///< Request for Shared data (Read)
    GETM,    ///< Request for Modified data (Write)
    PUTS,    ///< Shared line eviction
    PUTM,    ///< Modified line eviction (Write-back)
    DATA,    ///< Data response containing a block
    ACK,     ///< Invalidation acknowledgment
    INV,     ///< Invalidation request from Directory
    FWD_GETS,///< Forwarded GETS (from Dir to owner)
    FWD_GETM,///< Forwarded GETM (from Dir to owner)
    PUT_ACK  ///< Acknowledgment of eviction
};

/**
 * @brief Represents a message sent over the network between components.
 */
struct Message {
    MessageType type;      ///< The type of message.
    int sender_id;         ///< ID of the sending component.
    int receiver_id;       ///< ID of the receiving component.
    uint64_t address;      ///< Memory address associated with the message.
    int ack_count = 0;     ///< Number of ACKs a cache should expect (passed in DATA msg).
    std::string resolved_by = "Memory"; ///< Tracks which component resolved the request.

    Message(MessageType t, int s, int r, uint64_t addr, int acks = 0)
        : type(t), sender_id(s), receiver_id(r), address(addr), ack_count(acks) {}
};

/**
 * @brief Simulates a message-passing network with configurable variable latency.
 */
class Network {
private:
    EventScheduler& scheduler;
    std::mt19937 rng;
    std::uniform_int_distribution<uint64_t> dist;

public:
    /**
     * @brief Construct a new Network.
     * @param min_lat Minimum latency in cycles.
     * @param max_lat Maximum latency in cycles.
     */
    Network(EventScheduler& sched, uint64_t min_lat, uint64_t max_lat)
        : scheduler(sched), rng(42), dist(min_lat, max_lat) {}

    /**
     * @brief Sends a message asynchronously.
     */
    void send(Message msg, std::function<void(Message)> on_receive) {
        uint64_t latency = dist(rng);
        scheduler.schedule(scheduler.getTime() + latency, [msg, on_receive]() {
            if (on_receive) on_receive(msg);
        });
    }
};

} // namespace sim
