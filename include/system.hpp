#pragma once
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include "scheduler.hpp"
#include "cache.hpp"
#include "directory.hpp"
#include "message.hpp"
#include "memory.hpp"
#include <yaml-cpp/yaml.h>

namespace sim {

enum class AccessType { READ, WRITE };

class System {
public:
    EventScheduler scheduler;
    Network network;
    std::vector<std::shared_ptr<Cache>> l1_caches;
    std::vector<std::shared_ptr<Cache>> l2_caches;
    std::shared_ptr<Cache> l3_cache;
    std::shared_ptr<Directory> directory;
    std::shared_ptr<Memory> memory;

    uint32_t num_cores;
    uint32_t l2_group_size;
    uint64_t memory_latency;

    std::ofstream trace_file;

    System(uint32_t cores, uint32_t l2_group, uint64_t mem_lat);
    System(const YAML::Node& config); // New constructor
    ~System();

    void detectMemAccess(int core_id, AccessType type, uint64_t addr);
    void detectMemAccessBlocking(int core_id, AccessType type, uint64_t addr);
    void handleMessage(Message msg);

    void logAccess(int core_id, AccessType type, uint64_t addr, std::string resolved_by, uint64_t latency);

    int getL2Id(int core_id) { return core_id / l2_group_size; }
};

} // namespace sim
