#include "wrapper.hpp"
#include <iostream>
#include <yaml-cpp/yaml.h>

using namespace sim;

int main() {
    System sys(1, 1, 100);

    int a_val = 10;
    int b_val = 20;

    MemWrapper<int> a(&a_val, 0, &sys);
    MemWrapper<int> b(&b_val, 0, &sys);

    std::cout << "Executing: *a = *b\n";
    *a = *b;

    std::cout << "Result: a = " << a_val << std::endl;

    return 0;
}
