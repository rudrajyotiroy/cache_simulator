#include "system.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>
#include <vector>

using namespace sim;

/**
 * @brief Tests the polymorphic replacement policy implementation.
 */
void test_policies() {
    std::cout << "Testing Replacement Policies...\n";
    {
        LRUCache cache(0, 256, 4, 64, 1, "WriteBack", "L1");
        cache.updateOnAccess(0, 0);
        cache.updateOnAccess(0, 1);
        cache.updateOnAccess(0, 2);
        cache.updateOnAccess(0, 3);
        assert(cache.findVictim(0) == 0);
        std::cout << "  LRU passed\n";
    }
    {
        PLRUCache cache(0, 256, 4, 64, 1, "WriteBack", "L1");
        assert(cache.findVictim(0) == 0);
        cache.updateOnAccess(0, 0);
        assert(cache.findVictim(0) == 2);
        std::cout << "  PLRU passed\n";
    }
}

/**
 * @brief Tests the MSI coherence protocol transitions.
 */
void test_msi() {
    std::cout << "Testing MSI Protocol...\n";
    System sys(2, 1, 100);

    sys.detectMemAccessBlocking(0, AccessType::READ, 0x200);
    assert(sys.l1_caches[0]->getState(0x200) == Cache::getSState());

    sys.detectMemAccessBlocking(1, AccessType::WRITE, 0x200);
    assert(sys.l1_caches[1]->getState(0x200) == Cache::getMState());
    assert(sys.l1_caches[0]->getState(0x200) == Cache::getIState());
    std::cout << "  MSI passed\n";
}

/**
 * @brief Tests stalling of overlapping requests to the same block.
 */
void test_stalling() {
    std::cout << "Testing Stalling...\n";
    System sys(1, 1, 100);

    // Issue three reads to the same address simultaneously
    sys.detectMemAccess(0, AccessType::READ, 0x500);
    sys.detectMemAccess(0, AccessType::READ, 0x500);
    sys.detectMemAccess(0, AccessType::READ, 0x500);

    sys.scheduler.run();

    // If stalling works, the line should be in Shared state and all three should be logged.
    assert(sys.l1_caches[0]->getState(0x500) == Cache::getSState());
    std::cout << "  Stalling passed (check trace.csv for 3 resolution entries)\n";
}

/**
 * @brief Tests cache line eviction and directory update.
 */
void test_eviction() {
    std::cout << "Testing Eviction...\n";
    System sys(1, 1, 100);

    sys.l1_caches[0] = std::make_shared<LRUCache>(0, 64, 1, 64, 1, "WriteBack", "L1");
    sys.l1_caches[0]->next_level_id = 999;

    sys.detectMemAccessBlocking(0, AccessType::WRITE, 0x100);
    assert(sys.l1_caches[0]->getState(0x100) == Cache::getMState());
    assert(sys.directory->getEntry(0x100).state == DirState::M);

    sys.detectMemAccessBlocking(0, AccessType::READ, 0x200);
    assert(sys.l1_caches[0]->getState(0x100) == Cache::getIState());
    assert(sys.directory->getEntry(0x100).state == DirState::I);
    assert(sys.l1_caches[0]->getState(0x200) == Cache::getSState());
    std::cout << "  Eviction passed\n";
}

/**
 * @brief Tests the Write-Through policy.
 */
void test_write_through() {
    std::cout << "Testing Write-Through...\n";
    System sys(1, 1, 100);
    sys.l1_caches[0] = std::make_shared<LRUCache>(0, 32768, 8, 64, 1, "WriteThrough", "L1");
    sys.l1_caches[0]->next_level_id = 999;

    sys.detectMemAccessBlocking(0, AccessType::WRITE, 0x300);
    auto state = sys.l1_caches[0]->getState(0x300);
    auto dir_state = sys.directory->getEntry(0x300).state;

    assert(state == Cache::getMState());
    assert(dir_state == DirState::I);
    std::cout << "  Write-Through passed\n";
}

int main() {
    test_policies();
    test_msi();
    test_stalling();
    test_eviction();
    test_write_through();
    std::cout << "All tests passed!\n";
    return 0;
}
