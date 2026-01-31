#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <list>
#include <cmath>

namespace sim {

class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual uint32_t victim() = 0;
    virtual void access(uint32_t index) = 0;
    virtual void update_on_insert(uint32_t index) { access(index); }
};

class LRUPolicy : public ReplacementPolicy {
    std::list<uint32_t> lru_list;
    std::vector<std::list<uint32_t>::iterator> iters;
public:
    LRUPolicy(uint32_t assoc) : iters(assoc, lru_list.end()) {
        for (uint32_t i = 0; i < assoc; ++i) {
            lru_list.push_back(i);
            iters[i] = std::prev(lru_list.end());
        }
    }
    uint32_t victim() override { return lru_list.back(); }
    void access(uint32_t index) override {
        lru_list.erase(iters[index]);
        lru_list.push_front(index);
        iters[index] = lru_list.begin();
    }
};

class FIFOPolicy : public ReplacementPolicy {
    std::list<uint32_t> fifo_list;
public:
    FIFOPolicy(uint32_t assoc) {
        for (uint32_t i = 0; i < assoc; ++i) fifo_list.push_back(i);
    }
    uint32_t victim() override { return fifo_list.back(); }
    void access(uint32_t index) override {}
    void update_on_insert(uint32_t index) override {
        auto it = std::find(fifo_list.begin(), fifo_list.end(), index);
        if (it != fifo_list.end()) fifo_list.erase(it);
        fifo_list.push_front(index);
    }
};

class LFUPolicy : public ReplacementPolicy {
    std::vector<uint32_t> counts;
public:
    LFUPolicy(uint32_t assoc) : counts(assoc, 0) {}
    uint32_t victim() override {
        return std::distance(counts.begin(), std::min_element(counts.begin(), counts.end()));
    }
    void access(uint32_t index) override { counts[index]++; }
    void update_on_insert(uint32_t index) override { counts[index] = 1; }
};

class MRUPolicy : public ReplacementPolicy {
    std::list<uint32_t> mru_list;
    std::vector<std::list<uint32_t>::iterator> iters;
public:
    MRUPolicy(uint32_t assoc) : iters(assoc, mru_list.end()) {
        for (uint32_t i = 0; i < assoc; ++i) {
            mru_list.push_back(i);
            iters[i] = std::prev(mru_list.end());
        }
    }
    uint32_t victim() override { return mru_list.front(); }
    void access(uint32_t index) override {
        mru_list.erase(iters[index]);
        mru_list.push_front(index);
        iters[index] = mru_list.begin();
    }
};

class PLRUPolicy : public ReplacementPolicy {
    std::vector<bool> bits;
    uint32_t associativity;
    uint32_t num_levels;
public:
    PLRUPolicy(uint32_t assoc) : associativity(assoc), bits(assoc - 1, false) {
        num_levels = 0;
        uint32_t temp = assoc;
        while (temp > 1) { temp >>= 1; num_levels++; }
    }

    uint32_t victim() override {
        uint32_t curr = 0;
        uint32_t res = 0;
        for (uint32_t i = 0; i < num_levels; ++i) {
            if (bits[curr]) {
                res |= (1u << (num_levels - 1 - i));
                curr = 2 * curr + 2;
            } else {
                curr = 2 * curr + 1;
            }
        }
        return res;
    }

    void access(uint32_t index) override {
        uint32_t curr = 0;
        for (uint32_t i = 0; i < num_levels; ++i) {
            bool bit = (index >> (num_levels - 1 - i)) & 1;
            bits[curr] = !bit;
            curr = 2 * curr + (bit ? 2 : 1);
        }
    }
};

} // namespace sim
