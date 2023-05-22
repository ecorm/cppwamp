/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_METAAPI_HPP
#define CPPWAMP_INTERNAL_METAAPI_HPP

#include <algorithm>
#include <array>
#include <set>
#include <utility>
#include "../realmobserver.hpp"
#include "../rpcinfo.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TContext>
class MetaApiProvider
{
public:
    using Context = TContext;

    MetaApiProvider(Context* realm) :
        handlers_(
        {
            {"wamp.session.count", &MetaApiProvider::sessionCount},
            {"wamp.session.get",   &MetaApiProvider::sessionDetails},
            {"wamp.session.list",  &MetaApiProvider::sessionList},
        }),
        context_(*realm)
    {}

    bool call(RouterSession::Ptr caller, Rpc&& rpc)
    {
        auto iter = std::lower_bound(handlers_.begin, handlers_.end(),
                                     rpc.uri());
        if (iter == handlers_.end() || (iter->first != rpc.uri()))
            return false;

        auto requestId = rpc.requestId({});
        auto handler = iter->second;


        Outcome outcome;
        try
        {
            outcome = this->*(handler);
        }
        catch (const error::BadType& e)
        {
            outcome = Error{e};
        }

        if (outcome.type() == Outcome::Type::result)
        {
            Result result{std::move(outcome).asResult()};
            result.setRequestId({}, requestId);
            caller->sendRouterCommand(std::move(result), true);
        }
        else
        {
            assert(outcome.type() == Outcome::Type::error);
            Error error{std::move(outcome).asError()};
            error.setRequestId({}, rpc.requestId({}));
            caller->sendRouterCommand(std::move(error), true);
        }

        return true;
    }

private:
    typedef Outcome (MetaApiProvider::*Handler)(Rpc& rpc);

    static std::set<std::string> parseAuthRoles(const Rpc& rpc)
    {
        Array authRoleArray;
        rpc.convertTo(authRoleArray);
        std::set<std::string> authRoles;
        for (auto& elem: authRoleArray)
            authRoles.emplace(std::move(elem.as<String>()));
        return authRoles;
    }

    static Object toObject(const SessionDetails& details)
    {
        const auto& authInfo = *(details.authInfo);
        return Object
        {
            {"authid",       authInfo.id()},
            {"authmethod",   authInfo.method()},
            {"authprovider", authInfo.provider()},
            {"authrole",     authInfo.role()},
            {"session",      details.id}
            // TODO: transport
        };
    }

    Outcome sessionCount(Rpc& rpc)
    {
        if (rpc.args().empty())
            return Result{context_.sessionCount(nullptr)};

        auto authRoles = parseAuthRoles(rpc);
        auto filter =
            [&authRoles](SessionDetails session) -> bool
            {
                const auto& role = session.authInfo->role();
                return authRoles.count(role) != 0;
            };
        return Result{context_.sessionCount(filter)};
    }

    Outcome sessionList(Rpc& rpc)
    {
        if (rpc.args().empty())
            return Result{context_.sessionCount(nullptr)};

        auto authRoles = parseAuthRoles(rpc);
        auto filter =
            [&authRoles](SessionDetails session) -> bool
        {
            const auto& role = session.authInfo->role();
            return authRoles.count(role) != 0;
        };

        return Result{context_.sessionList(filter)};
    }

    Outcome sessionDetails(Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        auto details = context_.sessionDetails(sid);
        if (!details)
            return Error{details.error()};
        return Result{toObject(*details)};
    }

    std::array<std::pair<Uri, Handler>, 10> handlers_;
    Context& context_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_METAAPI_HPP
