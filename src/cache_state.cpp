#include "cache_state.hpp"
#include "cache.hpp"
#include "system.hpp"
#include "write_policy.hpp"

namespace sim {

class IState : public MSIState {
public:
    std::string name() const override { return "I"; }
    void onRead(Cache* cache, CacheLine* line, System* sys) override {
        if (line->pending_requests.size() == 1) {
            line->state = Cache::getISDState();
            sys->network.send(Message(MessageType::GETS, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
        }
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        if (line->pending_requests.size() == 1) {
            line->state = Cache::getIMADState();
            sys->network.send(Message(MessageType::GETM, cache->id, cache->next_level_id, line->addr), [sys](Message m){ sys->handleMessage(m); });
        }
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
        cache->processPendingRequests(line, sys);
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        // Upgrade: S -> IM_A
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
        cache->processPendingRequests(line, sys);
    }
    void onWrite(Cache* cache, CacheLine* line, System* sys) override {
        cache->processPendingRequests(line, sys);
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
        cache->processPendingRequests(line, sys);
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
            cache->processPendingRequests(line, sys);
        } else {
            line->state = Cache::getIMAState();
        }
    }
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->pending_acks--;
        if (line->pending_acks <= 0) {
            // Usually we still need data, so we stay in IM_AD? No, IM_AD means waiting for AD (both).
            // If we got all Acks but no Data, we move to IM_D?
            // Let's simplify: if we get all acks and we already had data (from previous state) or we are waiting for data.
            // My Directory protocol always sends Data with the last Ack count.
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
    void onData(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        // Handle DATA if it arrives in IMAState (e.g. from Directory upgrade response)
        line->pending_acks += msg.ack_count;
        if (line->pending_acks <= 0) {
            line->state = Cache::getMState();
            cache->processPendingRequests(line, sys);
        }
    }
    void onAck(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        line->pending_acks--;
        if (line->pending_acks <= 0) {
            line->state = Cache::getMState();
            cache->processPendingRequests(line, sys);
        }
    }
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        // If we are waiting for acks for our own write, but someone else invalidates us?
        // This is a race. In simple MSI, Directory handles the ordering.
        line->state = Cache::getIState();
        sys->network.send(Message(MessageType::ACK, cache->id, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
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
    void onInv(Cache* cache, CacheLine* line, System* sys, const Message& msg) override {
        // Someone invalidated while we were evicting. Just ACK.
        sys->network.send(Message(MessageType::ACK, cache->id, msg.sender_id, msg.address), [sys](Message m){ sys->handleMessage(m); });
    }
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
        cache->processPendingRequests(line, sys);
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
        cache->processPendingRequests(line, sys);
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
