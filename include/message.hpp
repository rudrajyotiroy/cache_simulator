#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <functional>
#include "scheduler.hpp"

namespace sim {

enum class MessageType {
    GETS,
    GETM,
    PUTS,
    PUTM,
    DATA,
    ACK,
    INV,
    FWD_GETS,
    FWD_GETM,
    PUT_ACK
};

struct Message {
    MessageType type;
    int sender_id;
    int receiver_id;
    uint64_t address;
    int ack_count = 0;
    std::string resolved_by = "Memory";

    Message(MessageType t, int s, int r, uint64_t addr, int acks = 0)
        : type(t), sender_id(s), receiver_id(r), address(addr), ack_count(acks) {}
};

class Network {
private:
    EventScheduler& scheduler;
    std::mt19937 rng;
    std::uniform_int_distribution<uint64_t> dist;

public:
    Network(EventScheduler& sched, uint64_t min_lat, uint64_t max_lat)
        : scheduler(sched), rng(42), dist(min_lat, max_lat) {}

    void send(Message msg, std::function<void(Message)> on_receive) {
        uint64_t latency = dist(rng);
        scheduler.schedule(scheduler.getTime() + latency, [msg, on_receive]() {
            if (on_receive) on_receive(msg);
        });
    }
};

} // namespace sim
