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
            {"wamp.session.count",            &Self::sessionCount},
            {"wamp.session.get",              &Self::sessionDetails},
            {"wamp.session.kill",             &Self::killSession},
            {"wamp.session.kill_all",         &Self::killAllSessions},
            {"wamp.session.kill_by_authid",   &Self::killSessionsByAuthId},
            {"wamp.session.kill_by_authrole", &Self::killSessionsByAuthRole},
            {"wamp.session.list",             &Self::sessionList},
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
            outcome = (this->*(handler))(*caller, rpc);
        }
        catch (Error& e)
        {
            outcome = std::move(e);
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
    using Self = MetaApiProvider;
    typedef Outcome (MetaApiProvider::*Handler)(RouterSession&, Rpc&);

    static std::set<std::string> parseAuthRoles(const Rpc& rpc)
    {
        Array authRoleArray;
        rpc.convertTo(authRoleArray);
        std::set<std::string> authRoles;
        for (auto& elem: authRoleArray)
            authRoles.emplace(std::move(elem.as<String>()));
        return authRoles;
    }

    static Reason parseReason(Rpc& rpc)
    {
        auto unex = makeUnexpectedError(MiscErrc::badType);

        auto reasonArg = std::move(rpc).kwargAs<String>("reason");
        if (reasonArg == unex)
        {
            throw Error{WampErrc::invalidArgument}
                .withArgs("'reason' argument must be a string");
        }
        if (reasonArg->empty())
        {
            throw Error{WampErrc::invalidUri}
                .withArgs("'reason' argument cannot be empty");
        }

        auto messageArg = std::move(rpc).kwargAs<String>("message");
        if (messageArg == unex)
        {
            throw Error{WampErrc::invalidArgument}
                .withArgs("'message' argument must be a string");
        }

        String reasonUri{errorCodeToUri(WampErrc::sessionKilled)};
        if (reasonArg)
            reasonUri = std::move(*reasonArg);
        Reason reason{std::move(reasonUri)};

        if (messageArg && !messageArg->empty())
            reason.withHint(std::move(*messageArg));

        return reason;
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

    Outcome sessionCount(RouterSession&, Rpc& rpc)
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

    Outcome sessionList(RouterSession&, Rpc& rpc)
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

    Outcome sessionDetails(RouterSession&, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        auto details = context_.sessionDetails(sid);
        if (!details)
            return Error{details.error()};
        return Result{toObject(*details)};
    }

    Outcome killSession(RouterSession& caller, Rpc& rpc)
    {
        SessionId sid = 0;
        rpc.convertTo(sid);
        if (sid == caller.wampId())
            return Error{WampErrc::noSuchSession};

        auto reason = parseReason(rpc);
        bool killed = context_.doKillSession(sid, std::move(reason));
        if (!killed)
            return Error{WampErrc::noSuchSession};
    }

    template <typename TFilter>
    std::vector<SessionId> killSessions(Rpc& rpc, TFilter&& filter)
    {
        auto reason = parseReason(rpc);
        return context_.doKillSessions(filter, reason);
    }

    Outcome killSessionsByAuthId(RouterSession& caller, Rpc& rpc)
    {
        String authId;
        auto ownId = caller.wampId();
        rpc.convertTo(authId);
        auto killed = killSessions(
            rpc,
            [&authId, ownId](SessionDetails s) -> bool
                {return (s.id != ownId) && (s.authInfo->id() == authId);});
        return Result{std::move(killed)};
    }

    Outcome killSessionsByAuthRole(RouterSession& caller, Rpc& rpc)
    {
        String authRole;
        auto ownId = caller.wampId();
        rpc.convertTo(authRole);
        auto killed = killSessions(
            rpc,
            [&authRole, ownId](SessionDetails s) -> bool
                {return (s.id != ownId) && (s.authInfo->role() == authRole);});
        return Result{killed.size()};
    }

    Outcome killAllSessions(RouterSession& caller, Rpc& rpc)
    {
        auto ownId = caller.wampId();
        auto killed = killSessions(
            rpc,
            [ownId](SessionDetails s) -> bool {return s.id != ownId;});
        return Result{killed.size()};
    }

    std::array<std::pair<Uri, Handler>, 10> handlers_;
    Context& context_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_METAAPI_HPP
