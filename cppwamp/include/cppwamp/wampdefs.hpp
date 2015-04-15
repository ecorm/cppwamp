/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSION_STATE_HPP
#define CPPWAMP_SESSION_STATE_HPP

//------------------------------------------------------------------------------
/** @file
    Contains type definitions related to WAMP IDs and sessions. */
//------------------------------------------------------------------------------

#include <cstdint>

namespace wamp
{

using SessionId      = int64_t; ///< Ephemeral ID associated with a WAMP session
using RequestId      = int64_t; ///< Ephemeral ID associated with a WAMP request
using SubscriptionId = int64_t; ///< Ephemeral ID associated with an topic subscription
using PublicationId  = int64_t; ///< Ephemeral ID associated with an event publication
using RegistrationId = int64_t; ///< Ephemeral ID associated with an RPC registration

/** Enumerates the possible states that a client or router session can be in. */
enum class SessionState
{
    disconnected, ///< The transport connection is not yet established
    connecting,   ///< Transport connection is in progress
    closed,       ///< Transport connected, but WAMP session is closed
    establishing, ///< WAMP session establishment is in progress
    established,  ///< WAMP session is established
    shuttingDown, ///< WAMP session is closing
    failed        ///< WAMP session or transport connection has failed
};

} // namespace wamp

#endif // CPPWAMP_SESSION_STATE_HPP
