#pragma once
#include <string>
#include "message.hpp"

namespace sim {

class Cache;
class System;
struct CacheLine;

enum class CoreAccess { READ, WRITE };

class MSIState {
public:
    virtual ~MSIState() = default;
    virtual std::string name() const = 0;

    virtual void onRead(Cache* cache, CacheLine* line, System* sys) = 0;
    virtual void onWrite(Cache* cache, CacheLine* line, System* sys) = 0;
    virtual void onReplacement(Cache* cache, CacheLine* line, System* sys) = 0;

    virtual void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
    virtual void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
    virtual void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
    virtual void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
    virtual void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
    virtual void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) = 0;
};

} // namespace sim
