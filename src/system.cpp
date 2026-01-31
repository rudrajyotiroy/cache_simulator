#include "system.hpp"
#include <iostream>
#include <iomanip>

namespace sim {

System::System(uint32_t cores, uint32_t l2_group, uint64_t mem_lat)
    : num_cores(cores), l2_group_size(l2_group), memory_latency(mem_lat),
      network(scheduler, 5, 15) {

    uint32_t num_l2 = (num_cores + l2_group_size - 1) / l2_group_size;
    int l2_start_id = num_cores;
    int l3_id = num_cores + num_l2;

    for (uint32_t i = 0; i < num_cores; ++i) {
        auto c = std::make_shared<Cache>(i, 32768, 8, 64, 1, "LRU", "WriteBack", "L1_" + std::to_string(i));
        c->next_level_id = l2_start_id + getL2Id(i);
        l1_caches.push_back(c);
    }

    for (uint32_t i = 0; i < num_l2; ++i) {
        auto c = std::make_shared<Cache>(l2_start_id + i, 262144, 8, 64, 10, "LRU", "WriteBack", "L2_" + std::to_string(i));
        c->next_level_id = l3_id;
        l2_caches.push_back(c);
    }

    l3_cache = std::make_shared<Cache>(l3_id, 2097152, 16, 64, 30, "LRU", "WriteBack", "L3");
    l3_cache->next_level_id = 999;

    directory = std::make_shared<Directory>();
    memory = std::make_shared<Memory>(mem_lat);

    trace_file.open("trace.csv");
    trace_file << "Core,Type,Address,ResolvedBy,Latency\n";
}

System::System(const YAML::Node& config)
    : num_cores(config["cores"].as<uint32_t>()),
      l2_group_size(config["l2_group_size"].as<uint32_t>()),
      memory_latency(config["memory_latency"].as<uint64_t>()),
      network(scheduler, 5, 15) {

    uint32_t num_l2 = (num_cores + l2_group_size - 1) / l2_group_size;
    int l2_start_id = num_cores;
    int l3_id = num_cores + num_l2;

    auto l1_cfg = config["l1"];
    auto l2_cfg = config["l2"];
    auto l3_cfg = config["l3"];

    for (uint32_t i = 0; i < num_cores; ++i) {
        auto c = std::make_shared<Cache>(i, l1_cfg["size"].as<uint32_t>(), l1_cfg["assoc"].as<uint32_t>(),
                                         64, 1, l1_cfg["repl"].as<std::string>(),
                                         l1_cfg["write_policy"] ? l1_cfg["write_policy"].as<std::string>() : "WriteBack",
                                         "L1_" + std::to_string(i));
        c->next_level_id = l2_start_id + getL2Id(i);
        l1_caches.push_back(c);
    }

    for (uint32_t i = 0; i < num_l2; ++i) {
        auto c = std::make_shared<Cache>(l2_start_id + i, l2_cfg["size"].as<uint32_t>(), l2_cfg["assoc"].as<uint32_t>(),
                                         64, 10, l2_cfg["repl"].as<std::string>(),
                                         l2_cfg["write_policy"] ? l2_cfg["write_policy"].as<std::string>() : "WriteBack",
                                         "L2_" + std::to_string(i));
        c->next_level_id = l3_id;
        l2_caches.push_back(c);
    }

    l3_cache = std::make_shared<Cache>(l3_id, l3_cfg["size"].as<uint32_t>(), l3_cfg["assoc"].as<uint32_t>(),
                                       64, 30, l3_cfg["repl"].as<std::string>(),
                                       l3_cfg["write_policy"] ? l3_cfg["write_policy"].as<std::string>() : "WriteBack",
                                       "L3");
    l3_cache->next_level_id = 999;

    directory = std::make_shared<Directory>();
    memory = std::make_shared<Memory>(memory_latency);

    trace_file.open("trace.csv");
    trace_file << "Core,Type,Address,ResolvedBy,Latency\n";
}

System::~System() {
    if (trace_file.is_open()) trace_file.close();
}

void System::logAccess(int core_id, AccessType type, uint64_t addr, std::string resolved_by, uint64_t latency) {
    trace_file << core_id << "," << (type == AccessType::READ ? "READ" : "WRITE") << ",0x"
               << std::hex << addr << std::dec << "," << resolved_by << "," << latency << "\n";
}

void System::handleMessage(Message msg) {
    if (msg.receiver_id == 999) {
        directory->handleMessage(msg, this);
    } else if (msg.receiver_id < (int)num_cores) {
        l1_caches[msg.receiver_id]->handleMessage(msg, this);
    } else if (msg.receiver_id < (int)(num_cores + l2_caches.size())) {
        l2_caches[msg.receiver_id - num_cores]->handleMessage(msg, this);
    } else if (msg.receiver_id == (int)(num_cores + l2_caches.size())) {
        l3_cache->handleMessage(msg, this);
    }
}

void System::detectMemAccess(int core_id, AccessType type, uint64_t addr) {
    uint64_t start_time = scheduler.getTime();
    auto callback = [this, core_id, type, addr, start_time](std::string resolved_by) {
        logAccess(core_id, type, addr, resolved_by, scheduler.getTime() - start_time);
    };

    if (type == AccessType::READ) {
        l1_caches[core_id]->coreRead(addr, this, callback);
    } else {
        l1_caches[core_id]->coreWrite(addr, this, callback);
    }
}

void System::detectMemAccessBlocking(int core_id, AccessType type, uint64_t addr) {
    detectMemAccess(core_id, type, addr);
    scheduler.run();
}

} // namespace sim
