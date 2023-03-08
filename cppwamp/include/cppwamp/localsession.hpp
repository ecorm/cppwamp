/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LOCALSESSION_HPP
#define CPPWAMP_LOCALSESSION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the LocalSession class. */
//------------------------------------------------------------------------------

#include "session.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class LocalSessionImpl; }


// TODO: Rewrite in terms of Session/Client using a Peer that talks directly
//       to router realm.
//------------------------------------------------------------------------------
class CPPWAMP_API LocalSession
{
public:
    LocalSession(std::shared_ptr<internal::LocalSessionImpl>) {}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/localsession.ipp"
#endif

#endif // CPPWAMP_LOCALSESSION_HPP
