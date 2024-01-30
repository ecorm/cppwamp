/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PASSKEY_HPP
#define CPPWAMP_PASSKEY_HPP

namespace wamp
{

class Abort;
class AuthorizationRequest;
class CalleeInputChunk;
class DirectRouterLink;
class Error;
class Event;
class HttpEndpoint;
class HttpJob;
class HttpServerBlock;
class Outcome;
class Session;
class Transporting;

namespace internal
{
    class PassKey
    {
        constexpr PassKey() {};

        friend class wamp::Abort;
        friend class wamp::AuthorizationRequest;
        friend class wamp::CalleeInputChunk;
        friend class wamp::DirectRouterLink;
        friend class wamp::Error;
        friend class wamp::Event;
        friend class wamp::HttpEndpoint;
        friend class wamp::HttpJob;
        friend class wamp::HttpServerBlock;
        friend class wamp::Outcome;
        friend class wamp::Session;
        friend class wamp::Transporting;
        template <typename> friend class BasicCalleeChannelImpl;
        template <typename> friend class BasicCallerChannelImpl;
        friend class Broker;
        friend class BrokerImpl;
        friend class BrokerPublication;
        friend class BrokerSubscribeRequest;
        friend class Client;
        friend class Dealer;
        friend class DealerImpl;
        friend class DealerJob;
        friend class DealerJobMap;
        friend class DirectPeer;
        friend class DirectRouterSession;
        friend class MatchUri;
        template <typename> friend class RawsockListener;
        template <typename> friend class RealmMetaProcedures;
        friend class MockWampClient;
        friend class MockWampServer;
        friend class NetworkPeer;
        friend class Peer;
        friend class ProcedureRegistry;
        friend class RequestIdChecker;
        friend class Requestor;
        friend class RouterImpl;
        friend class RouterServer;
        friend class RouterSession;
        friend class SessionInfoImpl;
        friend class ServerSession;
        friend class StreamRecord;
        friend class SubscriptionRecord;
    };

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_PASSKEY_HPP
