/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PASSKEY_HPP
#define CPPWAMP_PASSKEY_HPP

namespace wamp
{

class Event;
class Invocation;

namespace internal
{
    class PassKey
    {
        PassKey() {}

        friend class wamp::Event;
        friend class wamp::Invocation;
        friend class BrokerPublication;
        friend class BrokerUriAndPolicy;
        friend class Client;
        friend class DealerInvocation;
        friend class Invocation;
        friend class LocalSessionImpl;
        friend class Peer;
        friend class ServerSession;
        friend class RealmDealer;
        friend class RouterImpl;
        friend class RouterRealm;
        friend class RouterServer;
        friend class RouterSession;
    };
}

}

#endif // CPPWAMP_PASSKEY_HPP
