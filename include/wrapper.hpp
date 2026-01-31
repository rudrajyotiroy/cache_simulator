#pragma once
#include "system.hpp"

namespace sim {

template<typename T>
class MemWrapper {
private:
    T* ptr;
    uint64_t addr;
    int core_id;
    System* sys;

    struct Proxy {
        T* p;
        uint64_t a;
        int cid;
        System* s;

        operator T() const {
            s->detectMemAccessBlocking(cid, AccessType::READ, a);
            return *p;
        }

        Proxy& operator=(const T& other) {
            s->detectMemAccessBlocking(cid, AccessType::WRITE, a);
            *p = other;
            return *this;
        }

        Proxy& operator=(const Proxy& other) {
            T val = (T)other;
            s->detectMemAccessBlocking(cid, AccessType::WRITE, a);
            *p = val;
            return *this;
        }
    };

public:
    MemWrapper(T* p, int cid, System* s) : ptr(p), addr((uint64_t)p), core_id(cid), sys(s) {}

    Proxy operator*() { return {ptr, addr, core_id, sys}; }

    T* operator->() {
        sys->detectMemAccessBlocking(core_id, AccessType::READ, addr);
        return ptr;
    }
};

} // namespace sim
