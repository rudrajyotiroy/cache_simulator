#pragma once
#include <queue>
#include <functional>
#include <vector>
#include <cstdint>

namespace sim {

/**
 * @brief Represents a simulation event.
 */
class Event {
public:
    uint64_t time;       ///< Scheduled simulation time.
    uint32_t priority;   ///< Tie-breaking priority.
    std::function<void()> callback; ///< Action to perform.

    Event(uint64_t t, std::function<void()> cb, uint32_t p = 0)
        : time(t), priority(p), callback(cb) {}

    /**
     * @brief Comparator for priority queue (min-heap by time).
     */
    bool operator>(const Event& other) const {
        if (time != other.time) return time > other.time;
        return priority > other.priority;
    }
};

/**
 * @brief Manages the simulation timeline and event execution.
 */
class EventScheduler {
private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;
    uint64_t current_time = 0;

public:
    /**
     * @brief Schedules a new event.
     */
    void schedule(uint64_t time, std::function<void()> cb, uint32_t priority = 0) {
        events.emplace(time, cb, priority);
    }

    /**
     * @brief Pops and runs the earliest event.
     */
    void runNext() {
        if (!events.empty()) {
            Event top = events.top();
            events.pop();
            current_time = top.time;
            top.callback();
        }
    }

    /**
     * @brief Runs the simulation until the event queue is empty.
     */
    void run() {
        while (!events.empty()) {
            runNext();
        }
    }

    bool empty() const { return events.empty(); }
    uint64_t getTime() const { return current_time; }
};

} // namespace sim
