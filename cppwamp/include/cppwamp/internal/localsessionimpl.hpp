/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_LOCALSESSIONIMPL_HPP
#define CPPWAMP_INTERNAL_LOCALSESSIONIMPL_HPP

#include "client.hpp"

namespace wamp
{

namespace internal
{

// TODO: Rewrite in terms of Session/Client using a Peer that talks directly
//       to router realm.
//------------------------------------------------------------------------------
class LocalSessionImpl : public Client
{
public:
    using Ptr = std::shared_ptr<LocalSessionImpl>;

    template <typename... TIgnoredForNow>
    static Ptr create(TIgnoredForNow&&...) {return nullptr;}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_LOCALSESSIONIMPL_HPP
