/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PASSKEY_HPP
#define CPPWAMP_PASSKEY_HPP

namespace wamp
{

class Session;

namespace internal
{
    class PassKey
    {
        PassKey() {}

        friend class Peer;
        friend class Client;
        friend class wamp::Session; // TODO: Remove if unused
        friend class RouterSession;
    };
}

}

#endif // CPPWAMP_PASSKEY_HPP
