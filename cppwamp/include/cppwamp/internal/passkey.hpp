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
class Event;
class Invocation;
class Session;

namespace internal
{
    class PassKey
    {
        PassKey() {}

        // TODO: Verify these are all needed before next release
        friend class wamp::AuthorizationRequest;
        friend class wamp::Event;
        friend class wamp::Invocation;
        friend class wamp::Session;
        template <typename> friend class BasicCalleeChannelImpl;
        template <typename> friend class BasicCallerChannelImpl;
        friend class BrokerPublication;
        friend class Client;
        friend class Dealer;
        friend class DealerInvocation;
        friend class DealerRegistration;
        friend class DealerJob;
        friend class DealerJobMap;
        friend class Invocation;
        friend class LocalSessionImpl;
        friend class MatchUri;
        friend class Peer;
        friend class ProcedureRegistry;
        friend class Readership;
        friend class RealmSession;
        friend class Requestor;
        friend class RouterImpl;
        friend class RouterRealm;
        friend class RouterServer;
        friend class ServerSession;
        friend class StreamRecord;
    };
}

}

#endif // CPPWAMP_PASSKEY_HPP
