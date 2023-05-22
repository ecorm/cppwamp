/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PASSKEY_HPP
#define CPPWAMP_PASSKEY_HPP

namespace wamp
{

class AuthorizationRequest;
class CalleeInputChunk;
class DirectRouterLink;
class DirectSession;
class Error;
class Event;
class Interruption;
class Invocation;
class Outcome;
class Session;

namespace internal
{
    class PassKey
    {
        constexpr PassKey() {}

        // TODO: Verify these are all needed before next release
        friend class wamp::AuthorizationRequest;
        friend class wamp::CalleeInputChunk;
        friend class wamp::DirectRouterLink;
        friend class wamp::DirectSession;
        friend class wamp::Error;
        friend class wamp::Event;
        friend class wamp::Interruption;
        friend class wamp::Invocation;
        friend class wamp::Outcome;
        friend class wamp::Session;
        template <typename> friend class BasicCalleeChannelImpl;
        template <typename> friend class BasicCallerChannelImpl;
        friend class Broker;
        friend class BrokerPublication;
        friend class Client;
        friend class Dealer;
        friend class DealerInvocation;
        friend class DealerRegistration;
        friend class DealerJob;
        friend class DealerJobMap;
        friend class DirectPeer;
        friend class DirectRouterSession;
        friend class MatchUri;
        template <typename> friend class MetaApiProvider;
        friend class MockClient;
        friend class MockServer;
        friend class NetworkPeer;
        friend class Peer;
        friend class PeerListener;
        friend class ProcedureRegistry;
        friend class Readership;
        friend class RequestIdChecker;
        friend class Requestor;
        friend class RouterImpl;
        friend class RouterRealm;
        friend class RouterServer;
        friend class RouterSession;
        friend class ServerSession;
        friend class StreamRecord;
        friend class UriChecker;
    };
}

}

#endif // CPPWAMP_PASSKEY_HPP
