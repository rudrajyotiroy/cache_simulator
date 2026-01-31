#pragma once
#include <queue>
#include <functional>
#include <vector>
#include <cstdint>

namespace sim {

class Event {
public:
    uint64_t time;
    uint32_t priority;
    std::function<void()> callback;

    Event(uint64_t t, std::function<void()> cb, uint32_t p = 0)
        : time(t), priority(p), callback(cb) {}

    bool operator>(const Event& other) const {
        if (time != other.time) return time > other.time;
        return priority > other.priority;
    }
};

class EventScheduler {
private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;
    uint64_t current_time = 0;

public:
    void schedule(uint64_t time, std::function<void()> cb, uint32_t priority = 0) {
        events.emplace(time, cb, priority);
    }

    void runNext() {
        if (!events.empty()) {
            Event top = events.top();
            events.pop();
            current_time = top.time;
            top.callback();
        }
    }

    void run() {
        while (!events.empty()) {
            runNext();
        }
    }

    bool empty() const { return events.empty(); }
    uint64_t getTime() const { return current_time; }
};

} // namespace sim
