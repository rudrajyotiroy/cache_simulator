#include "system.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <yaml-cpp/yaml.h>

using namespace sim;

void runTrace(System& sys, const std::string& trace_path) {
    std::ifstream file(trace_path);
    std::string line;
    struct TraceEntry {
        uint64_t cycle;
        int core_id;
        AccessType type;
        uint64_t addr;
    };
    std::vector<TraceEntry> entries;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::stringstream ss(line);
        uint64_t cycle;
        int core_id;
        std::string type_str;
        std::string addr_str;

        if (!(ss >> cycle >> core_id >> type_str >> addr_str)) continue;

        uint64_t addr = std::stoull(addr_str, nullptr, 16);
        AccessType type = (type_str == "WRITE" ? AccessType::WRITE : AccessType::READ);
        entries.push_back({cycle, core_id, type, addr});
    }

    for (const auto& e : entries) {
        sys.scheduler.schedule(e.cycle, [&sys, e]() {
            sys.detectMemAccess(e.core_id, e.type, e.addr);
        });
    }

    sys.scheduler.run();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.yaml> [trace.csv]" << std::endl;
        return 1;
    }

    try {
        YAML::Node config = YAML::LoadFile(argv[1]);
        System sys(config);

        if (argc == 3) {
            runTrace(sys, argv[2]);
        } else {
            std::cout << "No trace file provided. System initialized." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
