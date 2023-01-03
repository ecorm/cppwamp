/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERREALM_HPP
#define CPPWAMP_INTERNAL_ROUTERREALM_HPP

#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include "../routerconfig.hpp"
#include "routercontext.hpp"
#include "routerserver.hpp"
#include "routersession.hpp"

namespace wamp
{


namespace internal
{

//------------------------------------------------------------------------------
class RealmBroker
{
public:
    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        UriAndPolicy topic{std::move(t)};
        if (!topic.uriIsValid())
            return makeUnexpectedError(SessionErrc::invalidUri);
        if (!topic.policyIsKnown())
            return makeUnexpectedError(SessionErrc::optionNotAllowed);

        auto sessionId = s->wampId();
        SubscriptionId subId = nullId();

        auto iter = subsByTopic_.find(topic);
        if (iter == subsByTopic_.end())
        {
            subId = nextSubId();
            SubInfo info{{{sessionId, std::move(s)}}, subId};
            iter = subsByTopic_.emplace(std::move(topic),
                                        std::move(info)).first;
            topicsBySubId_.emplace(subId, iter);
        }
        else
        {
            auto& info = iter->second;
            subId = info.subId;
            info.sessions.emplace(sessionId, std::move(s));
        }

        return subId;
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sid)
    {
        // TODO
    }

    ErrorOr<PublicationId> publish(Pub&& p, SessionId sid)
    {
        // TODO
    }

private:
    using Policy = Topic::MatchPolicy;

    struct UriAndPolicy
    {
        String uri;
        Topic::MatchPolicy policy;

        explicit UriAndPolicy(Topic&& t)
            : uri(std::move(t).uri({})),
              policy(t.matchPolicy())
        {}

        bool uriIsValid() const
        {
            // TODO:
            return true;
        }

        bool policyIsKnown() const {return policy != Policy::unknown;}

        bool operator<(const UriAndPolicy& rhs) const
        {
            return std::tie(uri, policy) < std::tie(rhs.uri, rhs.policy);
        }

        bool operator<(const String& topicUri) const
        {
            return uri < topicUri;
        }
    };

    struct SubInfo
    {
        std::map<SessionId, RouterSession::WeakPtr> sessions;
        SubscriptionId subId;
    };

    using SubsByTopic = std::map<UriAndPolicy, SubInfo>;

    SubscriptionId nextSubId()
    {
        auto s = nextSubscriptionId_;
        while ((s == nullId()) || (topicsBySubId_.count(s) == 1))
            ++s;
        nextSubscriptionId_ = s + 1;
        return s;
    }

    SubsByTopic subsByTopic_;
    std::map<SubscriptionId, SubsByTopic::iterator> topicsBySubId_;
    EphemeralId nextSubscriptionId_ = nullId();
    RandomIdGenerator pubIdGenerator_;
};

//------------------------------------------------------------------------------
class RealmDealer
{
public:
    ErrorOr<RegistrationId> enroll(Procedure&& p, RouterSession::Ptr s)
    {
        // TODO
    }

    ErrorOrDone unregister(RegistrationId rid, SessionId sid)
    {
        // TODO
    }

    ErrorOrDone call(Rpc&& p, SessionId sid)
    {
        // TODO
    }

    bool cancel(RequestId rid, SessionId sid)
    {
        // TODO
    }

    void yieldResult(Result&& r, SessionId sid)
    {
        // TODO
    }

    void yieldError(Error&& e, SessionId sid)
    {
        // TODO
    }
};

//------------------------------------------------------------------------------
class RouterRealm : public std::enable_shared_from_this<RouterRealm>
{
public:
    using Ptr = std::shared_ptr<RouterRealm>;
    using Executor = AnyIoExecutor;

    static Ptr create(Executor e, RealmConfig c, RouterContext r)
    {
        return Ptr(new RouterRealm(std::move(e), std::move(c), std::move(r)));
    }

    const IoStrand& strand() const {return strand_;}

    const std::string& uri() const {return config_.uri();}

    void join(RouterSession::Ptr session)
    {
        auto id = session->wampId();
        sessions_.emplace(id, std::move(session));
    }

    void close(bool terminate, Reason r)
    {
        std::string msg = terminate ? "Shutting down realm with reason "
                                    : "Terminating realm with reason ";
        msg += r.uri();
        if (!r.options().empty())
            msg += " " + toString(r.options());
        log({LogLevel::info, std::move(msg)});

        for (auto& kv: sessions_)
            kv.second->close(terminate, r);
        sessions_.clear();
    }

private:
    template <typename T>
    using FutureErrorOr = std::future<ErrorOr<T>>;

    using FutureErrorOrDone = std::future<ErrorOrDone>;

    RouterRealm(Executor&& e, RealmConfig&& c, RouterContext&& r)
        : strand_(boost::asio::make_strand(e)),
          router_(std::move(r)),
          config_(std::move(c)),
          logSuffix_(" (Realm " + config_.uri() + ")"),
          logger_(router_.logger())
    {}

    void log(LogEntry&& e)
    {
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    template <typename F>
    void dispatch(F&& f)
    {
        boost::asio::dispatch(strand_, std::forward<F>(f));
    }

    RouterLogger::Ptr logger() const {return logger_;}

    void safeLeave(RouterSession::Ptr s)
    {
        struct Dispatched
        {
            Ptr self;
            RouterSession::Ptr s;
            void operator()() {self->leave(std::move(s));}
        };

        dispatch(Dispatched{shared_from_this(), std::move(s)});
    }

    void leave(RouterSession::Ptr session)
    {
        sessions_.erase(session->wampId());
    }

    ErrorOr<SubscriptionId> subscribe(Topic&& t, RouterSession::Ptr s)
    {
        return broker_.subscribe(std::move(t), std::move(s));
    }

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sessionId)
    {
        return broker_.unsubscribe(subId, sessionId);
    }

    ErrorOr<PublicationId> publish(Pub&& pub, SessionId sid)
    {
        return broker_.publish(std::move(pub), sid);
    }

    ErrorOr<RegistrationId> enroll(Procedure&& proc, RouterSession::Ptr s)
    {
        return dealer_.enroll(std::move(proc), std::move(s));
    }

    ErrorOrDone unregister(RegistrationId rid, SessionId sid)
    {
        return dealer_.unregister(rid, sid);
    }

    ErrorOrDone call(Rpc&& rpc, SessionId sid)
    {
        return dealer_.call(std::move(rpc), sid);
    }

    bool cancelCall(RequestId rid, SessionId sid)
    {
        return dealer_.cancel(rid, sid);
    }

    void yieldResult(Result&& r, SessionId sid)
    {
        dealer_.yieldResult(std::move(r), sid);
    }

    void yieldError(Error&& e, SessionId sid)
    {
        dealer_.yieldError(std::move(e), sid);
    }

    IoStrand strand_;
    RouterContext router_;
    std::map<SessionId, RouterSession::Ptr> sessions_;
    RealmBroker broker_;
    RealmDealer dealer_;
    RealmConfig config_;
    std::string logSuffix_;
    RouterLogger::Ptr logger_;

    friend class RealmContext;
};


//******************************************************************************
// RealmContext
//******************************************************************************

inline RealmContext::RealmContext(std::shared_ptr<RouterRealm> r)
    : realm_(std::move(r))
{}

inline bool RealmContext::expired() const
{
    return realm_.expired();
}

inline IoStrand RealmContext::strand() const
{
    auto r = realm_.lock();
    if (r)
        return r->strand();
    return {};
}

inline RouterLogger::Ptr RealmContext::logger() const
{
    auto r = realm_.lock();
    if (r)
        return r->logger();
    return {};
}

inline void RealmContext::leave(std::shared_ptr<RouterSession> s)
{
    auto r = realm_.lock();
    if (r)
        r->safeLeave(std::move(s));
    realm_.reset();
}

inline RealmContext::FutureErrorOr<SubscriptionId>
RealmContext::subscribe(Topic t, RouterSessionPtr s)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<ErrorOr<SubscriptionId>> p;
        Topic t;
        RouterSession::Ptr s;

        void operator()()
        {
            try
            {
                p.set_value(realm->subscribe(std::move(t), std::move(s)));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOr<SubscriptionId>> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
    {
        realm->dispatch(Dispatched{realm, std::move(p), std::move(t),
                                   std::move(s)});
    }
    else
    {
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    }
    return fut;
}

inline RealmContext::FutureErrorOrDone
RealmContext::unsubscribe(SubscriptionId subId, SessionId sessionId)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<ErrorOrDone> p;
        SubscriptionId subId;
        SessionId sessionId;

        void operator()()
        {
            try
            {
                p.set_value(realm->unsubscribe(subId, sessionId));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOrDone> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(p), subId, sessionId});
    else
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    return fut;
}

inline RealmContext::FutureErrorOr<PublicationId>
RealmContext::publish(Pub pub, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<ErrorOr<PublicationId>> p;
        Pub pub;
        SessionId s;

        void operator()()
        {
            try
            {
                p.set_value(realm->publish(std::move(pub), s));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOr<PublicationId>> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(p), std::move(pub), sid});
    else
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    return fut;
}

inline RealmContext::FutureErrorOr<RegistrationId>
RealmContext::enroll(Procedure proc, RouterSessionPtr s)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<ErrorOr<RegistrationId>> p;
        Procedure proc;
        RouterSession::Ptr s;

        void operator()()
        {
            try
            {
                p.set_value(realm->enroll(std::move(proc), std::move(s)));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOr<RegistrationId>> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
    {
        realm->dispatch(Dispatched{realm, std::move(p), std::move(proc),
                                   std::move(s)});
    }
    else
    {
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    }
    return fut;
}

inline RealmContext::FutureErrorOrDone
RealmContext::unregister(RegistrationId rid, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr router;
        std::promise<ErrorOrDone> p;
        RegistrationId r;
        SessionId s;

        void operator()()
        {
            try
            {
                p.set_value(router->unregister(r, s));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOrDone> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(p), rid, sid});
    else
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    return fut;
}

inline RealmContext::FutureErrorOrDone
RealmContext::call(Rpc rpc, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<ErrorOrDone> p;
        Rpc r;
        SessionId s;

        void operator()()
        {
            try
            {
                p.set_value(realm->call(std::move(r), s));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<ErrorOrDone> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(p), std::move(rpc), sid});
    else
        p.set_value(makeUnexpectedError(SessionErrc::noSuchRealm));
    return fut;
}

inline std::future<bool> RealmContext::cancelCall(RequestId rid, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        std::promise<bool> p;
        RequestId r;
        SessionId s;

        void operator()()
        {
            try
            {
                p.set_value(realm->cancelCall(r, s));
            }
            catch (...)
            {
                p.set_exception(std::current_exception());
            }
        }
    };

    std::promise<bool> p;
    auto fut = p.get_future();
    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(p), rid, sid});
    else
        p.set_value(false);
    return fut;
}

inline void RealmContext::yieldResult(Result r, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        Result r;
        SessionId s;
        void operator()() {realm->yieldResult(std::move(r), s);}
    };

    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(r), sid});
}

inline void RealmContext::yieldError(Error e, SessionId sid)
{
    struct Dispatched
    {
        RouterRealm::Ptr realm;
        Error e;
        SessionId s;
        void operator()() {realm->yieldError(std::move(e), s);}
    };

    auto realm = realm_.lock();
    if (realm)
        realm->dispatch(Dispatched{realm, std::move(e), sid});
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERREALM_HPP
