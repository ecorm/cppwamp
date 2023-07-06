/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_AUTHORIZATIONLISTENER_HPP
#define CPPWAMP_INTERNAL_AUTHORIZATIONLISTENER_HPP

#include <memory>

namespace wamp
{

class Authorization;
class Procedure;
class Pub;
class Rpc;
class Topic;

namespace internal
{

class RouterSession;

//------------------------------------------------------------------------------
class AuthorizationListener
{
public:
    using WeakPtr = std::weak_ptr<AuthorizationListener>;
    using OriginatorPtr = std::shared_ptr<RouterSession>;

    virtual ~AuthorizationListener() = default;

    virtual void onAuthorized(const OriginatorPtr&, Topic&&) {};

    virtual void onAuthorized(const OriginatorPtr&, Pub&&) {};

    virtual void onAuthorized(const OriginatorPtr&, Procedure&&) {};

    virtual void onAuthorized(const OriginatorPtr&, Rpc&&) {};
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_AUTHORIZATIONLISTENER_HPP
