#include "wrapper.hpp"
#include <iostream>
#include <yaml-cpp/yaml.h>

using namespace sim;

/**
 * @brief Example C++ program using the memory instrumentation wrapper.
 *
 * This program demonstrates how regular C++ code can be instrumented
 * to trigger cache simulation events automatically.
 */
int main() {
    // Basic system setup: 1 core, 100 cycle memory latency
    System sys(1, 1, 100);

    int a_val = 10;
    int b_val = 20;

    // Wrap variables with MemWrapper. Core ID is set to 0.
    MemWrapper<int> a(&a_val, 0, &sys);
    MemWrapper<int> b(&b_val, 0, &sys);

    std::cout << "Executing: *a = *b (intercepted by simulator)\n";

    // This assignment triggers:
    // 1. A READ access on 'b'
    // 2. A WRITE access on 'a'
    // 3. Simulation of both accesses in the hierarchy.
    *a = *b;

    std::cout << "Result: a = " << a_val << std::endl;
    std::cout << "Trace has been generated in trace.csv\n";

    return 0;
}
