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
#include "../sessioninfo.hpp"
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

    virtual void onPeerMessage(Message&& m) = 0;

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
