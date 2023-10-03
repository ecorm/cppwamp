/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
#define CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP

#include <memory>
#include "../clientinfo.hpp"
#include "../routerlogger.hpp"
#include "../uri.hpp"
#include "random.hpp"

namespace wamp
{

class Authorization;

namespace internal
{

class ServerSession;
class RouterRealm;
class RouterSession;
class RouterImpl;

//------------------------------------------------------------------------------
class RealmContext
{
public:
    using RouterSessionPtr = std::shared_ptr<RouterSession>;

    RealmContext() = default;

    explicit RealmContext(const std::shared_ptr<RouterRealm>& r);

    bool expired() const;

    RouterLogger::Ptr logger() const;

    void reset();

    bool join(RouterSessionPtr session);

    // NOLINTNEXTLINE(bugprone-exception-escape)
    bool leave(const RouterSessionPtr& session) noexcept;

    template <typename C>
    bool send(RouterSessionPtr originator, C&& command);

private:
    std::weak_ptr<RouterRealm> realm_;
};

//------------------------------------------------------------------------------
class RouterContext
{
public:
    RouterContext();

    explicit RouterContext(const std::shared_ptr<RouterImpl>& r);

    bool expired() const;

    RouterLogger::Ptr logger() const;

    UriValidator::Ptr uriValidator() const;

    void reset();

    ReservedId reserveSessionId();

    RealmContext realmAt(const String& uri) const;

    bool closeRealm(const String& uri, Reason reason);

    uint64_t nextDirectSessionIndex();

private:
    std::weak_ptr<RouterImpl> router_;
    RandomIdPool::Ptr sessionIdPool_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
