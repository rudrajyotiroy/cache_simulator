#pragma once
#include "system.hpp"

namespace sim {

/**
 * @brief Template class to wrap arbitrary C++ variables for memory instrumentation.
 *
 * Uses operator overloading and a proxy object to intercept reads and writes,
 * automatically invoking the simulator's detectMemAccess hook.
 */
template<typename T>
class MemWrapper {
private:
    T* ptr;         ///< Pointer to the actual data.
    uint64_t addr;  ///< The virtual address (address of ptr).
    int core_id;    ///< ID of the core making the access.
    System* sys;    ///< Pointer to the simulation system.

    /**
     * @brief Proxy object returned by operator*. Intercepts assignments and reads.
     */
    struct Proxy {
        T* p;
        uint64_t a;
        int cid;
        System* s;

        /**
         * @brief Read access (*a).
         */
        operator T() const {
            s->detectMemAccessBlocking(cid, AccessType::READ, a);
            return *p;
        }

        /**
         * @brief Write access (*a = value).
         */
        Proxy& operator=(const T& other) {
            s->detectMemAccessBlocking(cid, AccessType::WRITE, a);
            *p = other;
            return *this;
        }

        /**
         * @brief Compound assignment (*a = *b).
         */
        Proxy& operator=(const Proxy& other) {
            T val = (T)other;
            s->detectMemAccessBlocking(cid, AccessType::WRITE, a);
            *p = val;
            return *this;
        }
    };

public:
    /**
     * @brief Construct a new MemWrapper.
     */
    MemWrapper(T* p, int cid, System* s) : ptr(p), addr((uint64_t)p), core_id(cid), sys(s) {}

    /**
     * @brief Overload dereference to return the proxy.
     */
    Proxy operator*() { return {ptr, addr, core_id, sys}; }

    /**
     * @brief Overload pointer access.
     */
    T* operator->() {
        sys->detectMemAccessBlocking(core_id, AccessType::READ, addr);
        return ptr;
    }
};

} // namespace sim
