#include "system.hpp"
#include "replacement_policy.hpp"
#include <iostream>
#include <cassert>
#include <cstdio>

using namespace sim;

void test_policies() {
    std::cout << "Testing Replacement Policies...\n";
    {
        LRUPolicy lru(4);
        lru.access(0); lru.access(1); lru.access(2); lru.access(3);
        assert(lru.victim() == 0);
        std::cout << "  LRU passed\n";
    }
    {
        PLRUPolicy plru(4);
        assert(plru.victim() == 0);
        plru.access(0);
        assert(plru.victim() == 2);
        std::cout << "  PLRU passed\n";
    }
}

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

void test_eviction() {
    std::cout << "Testing Eviction...\n";
    System sys(1, 1, 100);
    sys.l1_caches[0] = std::make_shared<Cache>(0, 64, 1, 64, 1, "LRU", "WriteBack", "L1");
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

void test_write_through() {
    std::cout << "Testing Write-Through...\n";
    System sys(1, 1, 100);
    sys.l1_caches[0] = std::make_shared<Cache>(0, 32768, 8, 64, 1, "LRU", "WriteThrough", "L1");
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
    test_eviction();
    test_write_through();
    std::cout << "All tests passed!\n";
    return 0;
}
