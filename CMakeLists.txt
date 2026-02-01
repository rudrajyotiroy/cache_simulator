cmake_minimum_required(VERSION 3.10)
project(CacheSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(yaml-cpp REQUIRED)

include_directories(include)

# Simulator library
add_library(simulator_lib
    src/cache.cpp
    src/cache_state.cpp
    src/write_policy.cpp
    src/directory.cpp
    src/system.cpp
    src/scheduler.cpp
    src/utils.cpp
)
target_link_libraries(simulator_lib yaml-cpp)

# Main executable for trace-based simulation
add_executable(cache_sim src/main.cpp)
target_link_libraries(cache_sim simulator_lib)

# Example program using the wrapper
add_executable(example_program src/example_program.cpp)
target_link_libraries(example_program simulator_lib)

# Tests
enable_testing()
add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests simulator_lib)
add_test(NAME AllTests COMMAND unit_tests)
