/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2018, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_WAMPDEFS_HPP
#define CPPWAMP_WAMPDEFS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains type definitions related to WAMP IDs and sessions. */
//------------------------------------------------------------------------------

#include <cstdint>

namespace wamp
{

using SessionId      = int64_t; ///< Ephemeral ID associated with a WAMP session
using RequestId      = int64_t; ///< Ephemeral ID associated with a WAMP request
using SubscriptionId = int64_t; ///< Ephemeral ID associated with an topic subscription
using PublicationId  = int64_t; ///< Ephemeral ID associated with an event publication
using RegistrationId = int64_t; ///< Ephemeral ID associated with an RPC registration

///< Obtains the value representing a blank RequestId.
constexpr RequestId nullRequestId() {return 0;}

//------------------------------------------------------------------------------
/** Enumerates the possible states that a client or router session can be in. */
//------------------------------------------------------------------------------
enum class SessionState
{
    disconnected,   ///< The transport connection is not yet established
    connecting,     ///< Transport connection is in progress
    closed,         ///< Transport connected, but WAMP session is closed
    establishing,   ///< WAMP session establishment is in progress
    authenticating, ///< WAMP authentication is in progress
    established,    ///< WAMP session is established
    shuttingDown,   ///< WAMP session is closing
    failed          ///< WAMP session or transport connection has failed
};

//------------------------------------------------------------------------------
/** Enumerates the possible call cancelling modes. */
//------------------------------------------------------------------------------
enum class CallCancelMode
{
    kill,       ///< INTERRUPT sent to callee; RESULT or ERROR returned, depending on callee
    killNoWait, ///< INTERRUPT sent to callee; router immediately returns ERROR
    skip        ///< No INTERRUPT sent to callee; router immediately returns ERROR
};

} // namespace wamp

#endif // CPPWAMP_WAMPDEFS_HPP
