#include "cache_state.hpp"
#include "cache.hpp"
#include "system.hpp"
#include "write_policy.hpp"

namespace sim {

class IState : public MSIState {
public:
    std::string name() const override { return "I"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {
        line->pending_access = CoreAccess::READ;
        line->state = Cache::getISDState();
        sys->network.send(Message(MessageType::GETS, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        line->pending_access = CoreAccess::WRITE;
        line->state = Cache::getIMADState();
        sys->network.send(Message(MessageType::GETM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
    }
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        sys->network.send(Message(MessageType::ACK, cache->id, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class SState : public MSIState {
public:
    std::string name() const override { return "S"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {
        if (line->on_fill) {
            auto cb = line->on_fill;
            line->on_fill = nullptr;
            sys->scheduler.schedule(sys->scheduler.getTime() + cache->lookup_latency, [cb, cache](){
                cb(cache->level_name);
            });
        }
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        line->pending_access = CoreAccess::WRITE;
        line->state = Cache::getIMAState();
        sys->network.send(Message(MessageType::GETM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
    }
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {
        line->state = Cache::getSIAState();
        sys->network.send(Message(MessageType::PUTS, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
    }
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getIState();
        sys->network.send(Message(MessageType::ACK, cache->id, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class MState : public MSIState {
public:
    std::string name() const override { return "M"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {
        if (line->on_fill) {
            auto cb = line->on_fill;
            line->on_fill = nullptr;
            sys->scheduler.schedule(sys->scheduler.getTime() + cache->lookup_latency, [cb, cache](){
                cb(cache->level_name);
            });
        }
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        cache->write_policy->onWrite(cache, line, sys);
        if (line->on_fill) {
            auto cb = line->on_fill;
            line->on_fill = nullptr;
            sys->scheduler.schedule(sys->scheduler.getTime() + cache->lookup_latency, [cb, cache](){
                cb(cache->level_name);
            });
        }
    }
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {
        line->state = Cache::getMIAState();
        sys->network.send(Message(MessageType::PUTM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
    }
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getSState();
        Message resp(MessageType::DATA, cache->id, msg.sender_id, msg.address);
        resp.resolved_by = cache->level_name;
        sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
        sys->network.send(Message(MessageType::DATA, cache->id, 999, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getIState();
        Message resp(MessageType::DATA, cache->id, msg.sender_id, msg.address);
        resp.resolved_by = cache->level_name;
        sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
    }
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class ISDState : public MSIState {
public:
    std::string name() const override { return "IS_D"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {}
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {}
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getSState();
        if (line->on_fill) { line->on_fill(msg.resolved_by); line->on_fill = nullptr; }
    }
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class IMADState : public MSIState {
public:
    std::string name() const override { return "IM_AD"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {}
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {}
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->pending_acks += msg.ack_count;
        if (line->pending_acks <= 0) {
            line->state = Cache::getMState();
            cache->write_policy->onWrite(cache, line, sys);
            if (line->on_fill) { line->on_fill(msg.resolved_by); line->on_fill = nullptr; }
        } else {
            line->state = Cache::getIMAState();
        }
    }
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->pending_acks--;
        if (line->pending_acks <= 0) {
            line->state = Cache::getMState();
            cache->write_policy->onWrite(cache, line, sys);
            if (line->on_fill) { line->on_fill("Coherence"); line->on_fill = nullptr; }
        }
    }
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class IMAState : public MSIState {
public:
    std::string name() const override { return "IM_A"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {}
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {}
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->pending_acks--;
        if (line->pending_acks <= 0) {
            line->state = Cache::getMState();
            cache->write_policy->onWrite(cache, line, sys);
            if (line->on_fill) { line->on_fill("Coherence"); line->on_fill = nullptr; }
        }
    }
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
};

class MIAState : public MSIState {
public:
    std::string name() const override { return "MI_A"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {}
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {}
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        Message resp(MessageType::DATA, cache->id, msg.sender_id, msg.address);
        resp.resolved_by = cache->level_name;
        sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
        sys->network.send(Message(MessageType::DATA, cache->id, 999, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        Message resp(MessageType::DATA, cache->id, msg.sender_id, msg.address);
        resp.resolved_by = cache->level_name;
        sys->network.send(resp, [sys](Message m){ sys->handleMessage(m); });
    }
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getIState();
    }
};

class SIAState : public MSIState {
public:
    std::string name() const override { return "SI_A"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {}
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {}
    void onReplacement(Cache* cache, CacheLine* line, System* sys) override {}
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getIState();
        sys->network.send(Message(MessageType::ACK, cache->id, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
    void onFwdGets(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onFwdGetm(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {}
    void onPutAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->state = Cache::getIState();
    }
};

static IState i_state;
static SState s_state;
static MState m_state;
static ISDState isd_state;
static IMADState imad_state;
static IMAState ima_state;
static MIAState mia_state;
static SIAState sia_state;

MSIState* Cache::getIState() { return &i_state; }
MSIState* Cache::getSState() { return &s_state; }
MSIState* Cache::getMState() { return &m_state; }
MSIState* Cache::getISDState() { return &isd_state; }
MSIState* Cache::getIMADState() { return &imad_state; }
MSIState* Cache::getIMAState() { return &ima_state; }
MSIState* Cache::getMIAState() { return &mia_state; }
MSIState* Cache::getSIAState() { return &sia_state; }

} // namespace sim
