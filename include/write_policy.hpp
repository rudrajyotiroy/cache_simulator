#pragma once
#include "message.hpp"

namespace sim {

class Cache;
class System;
struct CacheLine;

class WritePolicy {
public:
    virtual ~WritePolicy() = default;
    virtual void onWrite(Cache* cache, CacheLine* line, System* sys) = 0;
};

class WriteBack : public WritePolicy {
public:
    void onWrite(Cache* cache, CacheLine* line, System* sys) override;
};

class WriteThrough : public WritePolicy {
public:
    void onWrite(Cache* cache, CacheLine* line, System* sys) override;
};

} // namespace sim
