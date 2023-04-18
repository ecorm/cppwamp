/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PEERLISTENER_HPP
#define CPPWAMP_INTERNAL_PEERLISTENER_HPP

#include <atomic>
#include <cassert>
#include <string>
#include <utility>
#include "../errorinfo.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "../sessioninfo.hpp"
#include "commandinfo.hpp"
#include "message.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class PeerListener
{
public:
    virtual void onPeerDisconnect() = 0;

    virtual void onPeerFailure(std::error_code ec, bool abortSent,
                               std::string why = {}) = 0;

    virtual void onPeerTrace(std::string&& messageDump) = 0;

    virtual void onPeerHello(Realm&&) {assert(false);}

    virtual void onPeerWelcome(Welcome&& w)
    {
        onPeerMessage(std::move(w.message({})));
    }

    virtual void onPeerAbort(Reason&&, bool wasJoining) = 0;

    virtual void onPeerChallenge(Challenge&& c) {assert(false);}

    virtual void onPeerAuthenticate(Authentication&& c) {assert(false);}

    virtual void onPeerGoodbye(Reason&&, bool wasShuttingDown) = 0;

    virtual void onPeerMessage(Message&& m)
    {
        using K = MessageKind;
        switch (m.kind())
        {
        case K::error:          return onPeerCommand(Error{{},            std::move(m)});
        case K::publish:        return onPeerCommand(Pub{{},              std::move(m)});
        case K::published:      return onPeerCommand(Published{{},        std::move(m)});
        case K::subscribe:      return onPeerCommand(Topic{{},            std::move(m)});
        case K::subscribed:     return onPeerCommand(Subscribed{{},       std::move(m)});
        case K::unsubscribe:    return onPeerCommand(Unsubscribe{{},      std::move(m)});
        case K::unsubscribed:   return onPeerCommand(Unsubscribed{{},     std::move(m)});
        case K::event:          return onPeerCommand(Event{{},            std::move(m)});
        case K::call:           return onPeerCommand(Rpc{{},              std::move(m)});
        case K::cancel:         return onPeerCommand(CallCancellation{{}, std::move(m)});
        case K::result:         return onPeerCommand(Result{{},           std::move(m)});
        case K::enroll:         return onPeerCommand(Procedure{{},        std::move(m)});
        case K::registered:     return onPeerCommand(Registered{{},       std::move(m)});
        case K::unregister:     return onPeerCommand(Unregister{{},       std::move(m)});
        case K::unregistered:   return onPeerCommand(Unregistered{{},     std::move(m)});
        case K::invocation:     return onPeerCommand(Invocation{{},       std::move(m)});
        case K::interrupt:      return onPeerCommand(Interruption{{},     std::move(m)});
        case K::yield:          return onPeerCommand(Result{{},           std::move(m)});
        default: assert(false && "Unexpected MessageKind enumerator");
        }
    }

    virtual void onPeerCommand(Error&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Pub&& c)              {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Published&& c)        {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Topic&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Subscribed&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unsubscribe&& c)      {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unsubscribed&& c)     {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Event&& c)            {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Rpc&& c)              {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(CallCancellation&& c) {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Result&& c)           {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Procedure&& c)        {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Registered&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unregister&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Unregistered&& c)     {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Invocation&& c)       {onPeerMessage(std::move(c.message({})));}
    virtual void onPeerCommand(Interruption&& c)     {onPeerMessage(std::move(c.message({})));}

    void enableTracing(bool enabled = true) {traceEnabled_.store(enabled);}

    bool traceEnabled() const {return traceEnabled_.load();}

protected:
    PeerListener() : traceEnabled_(false) {}

private:
    std::atomic<bool> traceEnabled_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PEERLISTENER_HPP
